#include <drivers/virtio/virtio_net.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/string.h>

#define JOS_KERNEL
#include <kern/pmap.h>

static uint8_t rx_descr_buf[4 * PAGE_SIZE];
static uint8_t tx_descr_buf[4 * PAGE_SIZE];

static uint16_t processing_tx_desc = VIRTQ_INVALID_DESC;

int
virtio_net_setup_queues(VirtioNetDevice *virtio_net_dev) {
    assert(virtio_net_dev != NULL);

    VirtioDevice *virtio_dev = &virtio_net_dev->virtio_dev;

    int err = virtio_setup_queue(
            virtio_dev,
            &virtio_net_dev->rx_virtq,
            VIRTIO_NET_Q_RX,
            virtio_net_dev->rx_buffer,
            QUEUE_BUFFER_SIZE);
    if (err != 0) {
        cprintf("%s: Unable to setup RX queue\n", __func__);
        return err;
    }

    err = virtio_setup_queue(
            virtio_dev,
            &virtio_net_dev->tx_virtq,
            VIRTIO_NET_Q_TX,
            virtio_net_dev->tx_buffer,
            QUEUE_BUFFER_SIZE);
    if (err != 0) {
        cprintf("%s: Unable to setup TX queue\n", __func__);
        return err;
    }

    return 0;
}

static void 
fill_rx_descriptors(VirtioNetDevice *virtio_net_dev) {
    assert(virtio_net_dev);

    virtio_net_dev->rx_virtq.descriptor_table[0].address = (uint64_t)PADDR(rx_descr_buf);
    virtio_net_dev->rx_virtq.descriptor_table[0].length = sizeof(rx_descr_buf);
    virtio_net_dev->rx_virtq.descriptor_table[0].flags = VIRTQ_DESC_F_WRITE;
    virtio_net_dev->rx_virtq.descriptor_table[0].next = 0;

    virtio_net_dev->rx_virtq.available_ring.ring[0] = 0;
    *virtio_net_dev->rx_virtq.available_ring.idx = 1;
    *virtio_net_dev->rx_virtq.available_ring.flags = 0;

    virtio_notify(&virtio_net_dev->virtio_dev, VIRTIO_NET_Q_RX);
}

int
virtio_net_init(VirtioNetDevice *virtio_net_dev, PciDevice *pci_dev) {
    VirtioDevice *virtio_dev = &virtio_net_dev->virtio_dev;
    int err = virtio_init(virtio_dev, pci_dev);
    if (err != 0) {
        cprintf("%s: Unable to init virtio device\n", __func__);
        return err;
    }

    if (virtio_dev->id != VIRTIO_DEVICE_ID_NETWORK) {
        cprintf("%s: Virtio device %p isn't virtio network card\n", __func__, virtio_dev);
        return -E_UNSUPPORTED;
    }

    pci_dev_enable(pci_dev);

    virtio_reset(virtio_dev);
    virtio_set_status(virtio_dev, VSTAT_ACK);
    virtio_set_status(virtio_dev, VSTAT_ACK | VSTAT_DRIVER);

    uint32_t features = virtio_read_device_features(virtio_dev);
    features = features & REQUIRED_FEATURES;
    if (features != REQUIRED_FEATURES) {
        cprintf("%s: Device doesn't accept required features\n", __func__);
        return -E_UNSUPPORTED;
    }

    virtio_set_driver_features(virtio_dev, features);

    virtio_set_status(virtio_dev, VSTAT_ACK | VSTAT_DRIVER | VSTAT_FEATURES_OK);
    if ((virtio_read_status(virtio_dev) & VSTAT_FEATURES_OK) == 0) {
        cprintf("virtio_net: features were not accepted!\n");
        return -E_UNSUPPORTED;
    }

    uint8_t *raw_conf = (uint8_t *)&virtio_net_dev->net_config;
    for (size_t index = 0; index < sizeof(VirtioNetConfig); index++) {
        *raw_conf = virtio_read8(virtio_dev, VIRTIO_DEVICE_SPECIFIC_CONFIGURATION_OFFSET_PCI + index);
        ++raw_conf;
    }

    VirtioNetConfig *config = &virtio_net_dev->net_config;
    cprintf("%s: Virtio network device %p\n", __func__, virtio_net_dev);
    cprintf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
            config->mac[0], config->mac[1], config->mac[2],
            config->mac[3], config->mac[4], config->mac[5]);

    err = virtio_net_setup_queues(virtio_net_dev);
    if (err != 0) {
        cprintf("%s: Unable to setup queues\n", __func__);
        return err;
    }

    virtio_set_status(virtio_dev, VSTAT_ACK | VSTAT_DRIVER | VSTAT_FEATURES_OK | VSTAT_DRIVER_OK);

    fill_rx_descriptors(virtio_net_dev);
    return 0;
}

static void
rx_queue_desc_handler(Virtq *virtq, VirtqUsedElem *const used_elem, void *args) {
    assert(virtq);
    assert(used_elem);
    assert(args);

    VirtqDescriptor *desc = &virtq->descriptor_table[used_elem->id];

    const uint32_t msg_len = used_elem->len;
    assert("virtio-net: message len is out of bounds" && msg_len < desc->length);

    if (msg_len < sizeof(VirtioNetReq)) {
        cprintf("%s: too small packet recieved from device!\n", __func__);
        return;
    }

    const uint8_t *const msg_buf = (uint8_t*)desc->address;
    if (msg_buf == NULL) {
        cprintf("%s: received null descriptor address\n", __func__);
        return;
    }

    const uint8_t *const packet = msg_buf + sizeof(VirtioNetReq);
    const size_t packet_len = msg_len - sizeof(VirtioNetReq);

    recv_handler_t packet_receiver = (recv_handler_t)args;
    int err = packet_receiver((void*)packet, packet_len);
    if (err != 0) {
        cprintf("%s: failed to receive packet from virtio-net with err = %d\n", __func__, err);
    }

    uint16_t avail_ring_idx = *virtq->available_ring.idx;
    memory_fence();

    virtq->available_ring.ring[avail_ring_idx % virtq->queue_size] = used_elem->id;

    memory_fence();
    *virtq->available_ring.idx += 1;
}

int
virtio_net_handle_rx(VirtioNetDevice *virtio_net_dev, recv_handler_t packet_receiver) {
    assert(virtio_net_dev);
    int err = virtio_recycle_used(&virtio_net_dev->rx_virtq, rx_queue_desc_handler, packet_receiver);
    if (err != 0) {
        cprintf("%s: failed to recycle used virtio rings for rx queue with err = %d\n", __func__, err);
        return err;
    }

    virtio_notify(&virtio_net_dev->virtio_dev, VIRTIO_NET_Q_RX);
    return 0;
}

static void
tx_queue_desc_handler(Virtq *virtq, VirtqUsedElem *const used_elem, void *args) {
    assert(virtq);
    assert(used_elem);

    int err = virtq_free_buffer(virtq, used_elem->id);
    if (err != 0) {
        cprintf("%s: failed to free tx virtq buffer with err = %d\n", __func__, err);
        return;
    }

    if (used_elem->id == processing_tx_desc) {
        if (args != NULL) {
            sent_handler_t packet_sent_handler = (sent_handler_t)args;
            packet_sent_handler();
        }

        processing_tx_desc = VIRTQ_INVALID_DESC;
    }
}

int
virtio_net_handle_tx(VirtioNetDevice *virtio_net_dev, sent_handler_t packet_sent) {
    assert(virtio_net_dev);

    int err = virtio_recycle_used(&virtio_net_dev->tx_virtq, tx_queue_desc_handler, packet_sent);
    if (err != 0) {
        cprintf("%s: failed to recycle used virtio rings for tx queue with err = %d\n", __func__, err);
        return err;
    }

    return 0;
}

int
virtio_net_send_buffer(VirtioNetDevice *virtio_net_dev, void *const buf, const size_t len) {
    assert(virtio_net_dev);

    VirtioNetReq hdr = {};
    memset(&hdr, 0, sizeof(hdr));

    if (sizeof(hdr) + len > sizeof(tx_descr_buf)) {
        cprintf("virtio-net: message is too big!\n");
        return -E_NO_MEM;
    }

    memcpy(tx_descr_buf, &hdr, sizeof(hdr));
    memcpy(tx_descr_buf + sizeof(hdr), buf, len);

    int err = virtio_send_buffer(
        &virtio_net_dev->virtio_dev,
        &virtio_net_dev->tx_virtq,
        tx_descr_buf,
        len + sizeof(hdr),
        false,
        &processing_tx_desc
    );
    if (err != 0) {
        cprintf("%s: failed to send virtio buffer with err = %d\n", __func__, err);
        processing_tx_desc = VIRTQ_INVALID_DESC;
        return err;
    }

    return 0;
}
