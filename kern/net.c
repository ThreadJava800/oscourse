#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtio_net.h>

#include <inc/assert.h>
#include <inc/error.h>
#include <inc/net.h>
#include <inc/ringbuf.h>

#include <kern/picirq.h>
#include <kern/trap.h>

#define VIRTIO_ISR_CONFIG_CHANGE    0x80
#define VIRTIO_ISR_QUEUE_INTERRUPT  0x01

static VirtioNetDevice *net_dev = NULL;
static uint8_t virtio_net_irq_line = 0;

static uint8_t net_recv_data_buf[4 * PAGE_SIZE];
static Ringbuf net_recv_data_rb;

static bool net_data_received = 0;

static int net_recv_handler(void *buf, const size_t len) {
    assert(buf);

    if (len < sizeof(VirtioNetReq)) {
        cprintf("net: too small packet recieved from device!\n");
        return -E_INVAL;
    }

    const uint8_t *const packet = (uint8_t*)buf + sizeof(VirtioNetReq);
    const size_t packet_len = len - sizeof(VirtioNetReq);

    int err = rb_write(&net_recv_data_rb, packet, packet_len);
    if (err != 0) {
        cprintf("net: failed to write recieved packet to the ringbuf!\n");
        return err;
    }

    net_data_received = true; // signal to the reader
    return 0;
}

static void nic_int_handler() {
    assert(net_dev);
    assert(virtio_net_irq_line);

    const uint8_t isr = virtio_read_isr(&net_dev->virtio_dev);
    if (isr == 0) {
        return;
    }

    if (isr & VIRTIO_ISR_QUEUE_INTERRUPT) {
        int err = virtio_recv_buffer(&net_dev->virtio_dev, &net_dev->rx_virtq, net_recv_handler);
        if (err != 0) {
            cprintf("net: failed to recieve network buffer with err = %d\n", err);
        }
        // TODO: implement tx buffer processing
    }
    if (isr & VIRTIO_ISR_CONFIG_CHANGE) {
        panic("net: virtio-net config change ISR is not yet implemented\n");
    }

    pic_send_eoi(virtio_net_irq_line);
}

int init_network() {
    net_dev = get_virtio_net_dev();
    if (!net_dev) {
        cprintf("Failed to get virtio net device!\n");
        return -E_BAD_ENV;
    }

    virtio_net_irq_line = virtio_get_irq_line(&net_dev->virtio_dev);

    int err = rb_init(
        &net_recv_data_rb,
        net_recv_data_buf,
        sizeof(net_recv_data_buf),
        false
    );
    if (err != 0) {
        cprintf("Failed to initialize ringbuf with err = %d\n", err);
        return err;
    }

    err = add_device_irq_handler(virtio_net_irq_line, nic_int_handler);
    if (err != 0) {
        cprintf("Failed to add irq handler for virtio-net with err = %d\n", err);
        return err;
    }
    return 0;
}

static int net_copy_to_user_buf(void *buf, size_t *const size) {
    int err = rb_read(&net_recv_data_rb, (uint8_t*)buf, size);
    if (err != 0) {
        cprintf("net: failed to read data from ringbuffer with err = %d\n", err);
        return err;
    }

    // prior this write the interrupt could happen.
    // it is OK, as we still check for rb_is_empty in net_read
    net_data_received = false;
    return 0;
}

int net_read(void *buf, size_t *const size) {
    if (!rb_is_empty(&net_recv_data_rb)) {
        // return what we collected
        return net_copy_to_user_buf(buf, size);
    }

    while (!net_data_received) {
        cpu_pause();
    }
    return net_copy_to_user_buf(buf, size);
}

int net_write(void *buf, const size_t size) {
    panic("%s: not yet implemented!\n", __func__);
}

static int isprint(int c) {
    return c >= 32 && c <= 126;
}

void packet_dump(const uint8_t *const buf, const size_t buf_size) {
    for (size_t i = 0; i < buf_size; i += 16) {
        cprintf("%08zx  ", i);

        for (size_t j = 0; j < 16; ++j) {
            if (i + j < buf_size) {
                cprintf("%02x ", buf[i + j]);
            } else {
                cprintf("   ");
            }
        }

        cprintf(" |");
        for (size_t j = 0; j < 16; ++j) {
            if (i + j >= buf_size) {
                break;
            }

            const unsigned char c = buf[i + j];
            cprintf("%c", isprint(c) ? c : '.');
        }
        cprintf("|\n");
    }
}
