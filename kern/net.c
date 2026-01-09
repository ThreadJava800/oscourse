#include <drivers/virtio/virtio.h>
#include <drivers/virtio/virtio_net.h>

#include <inc/assert.h>
#include <inc/error.h>
#include <inc/net.h>

#include <kern/picirq.h>
#include <kern/trap.h>

#define VIRTIO_ISR_CONFIG_CHANGE    0x80
#define VIRTIO_ISR_QUEUE_INTERRUPT  0x01

static VirtioNetDevice *net_dev = NULL;
static uint8_t virtio_net_irq_line = 0;

static void nic_int_handler() {
    assert(net_dev);
    assert(virtio_net_irq_line);

    const uint8_t isr = read_virtio_net_isr(net_dev);
    if (isr == 0) {
        return;
    }

    if (isr & VIRTIO_ISR_QUEUE_INTERRUPT) {
        virtio_net_handle_rx(net_dev);
        virtio_net_handle_rx(net_dev);
    }
    if (isr & VIRTIO_ISR_CONFIG_CHANGE) {
        virtio_net_handle_config(net_dev);
    }

    pic_send_eoi(virtio_net_irq_line);
}

int init_network() {
    net_dev = get_virtio_net_dev();
    if (!net_dev) {
        cprintf("Failed to get virtio net device!\n");
        return -E_BAD_ENV;
    }

    virtio_net_irq_line = get_virtio_net_irq_line(net_dev);

    int err = add_device_irq_handler(virtio_net_irq_line, nic_int_handler);
    if (err != 0) {
        cprintf("Failed to add irq handler for virtio-net with err = %d\n", err);
        return err;
    }
    return 0;
}

int net_read(void *buf, size_t *size) {
    panic("%s: not yet implemented!\n", __func__);
}

int net_write(void *buf, size_t size) {
    panic("%s: not yet implemented!\n", __func__);
}
