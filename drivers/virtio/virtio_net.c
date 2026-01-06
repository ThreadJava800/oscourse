#include <drivers/virtio/virtio_net.h>
#include <inc/assert.h>
#include <inc/error.h>

int
virtio_net_setup_queues(VirtioNetDevice *virtio_net_dev) {
    assert(virtio_net_dev != NULL);

    VirtioDevice *virtio_dev = virtio_net_dev->virtio_dev;

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

int
virtio_net_init(VirtioNetDevice *virtio_net_dev, VirtioDevice *virtio_dev) {
    assert(virtio_dev != NULL);

    if (virtio_dev->id != VIRTIO_DEVICE_ID_NETWORK) {
        cprintf("%s: Virtio device %p isn't virtio network card\n", __func__, virtio_dev);
        return -E_UNSUPPORTED;
    }

    virtio_net_dev->virtio_dev = virtio_dev;
    virtio_reset(virtio_dev);
    virtio_set_status(virtio_dev, VSTAT_ACK | VSTAT_DRIVER);

    uint32_t features = virtio_read_device_features(virtio_dev);
    features = features & REQUIRED_FEATURES;
    if ((features & REQUIRED_FEATURES) != REQUIRED_FEATURES) {
        cprintf("%s: Device doesn't accept required features\n", __func__);
        return -E_UNSUPPORTED;
    }

    virtio_set_driver_features(virtio_dev, features);

    uint8_t *raw_conf = (uint8_t *)&virtio_net_dev->net_config;
    for (size_t index = 0; index < sizeof(VirtioNetConfig); index++) {
        *raw_conf = virtio_read8(virtio_dev, VIRTIO_DEVICE_SPECIFIC_CONFIGURATION_OFFSET_PCI + index);
    }

    VirtioNetConfig *config = &virtio_net_dev->net_config;
    cprintf("%s: Virtio network device %p\n", __func__, virtio_net_dev);
    cprintf("MAC: %02x:%02x:%02x:%02x:%02x:%02x\n",
            config->mac[0], config->mac[1], config->mac[2],
            config->mac[3], config->mac[4], config->mac[5]);

    int err = virtio_net_setup_queues(virtio_net_dev);
    if (err != 0) {
        cprintf("%s: Unable to setup queues\n", __func__);
        return err;
    }

    virtio_set_status(virtio_dev, VSTAT_ACK | VSTAT_DRIVER | VSTAT_DRIVER_OK);

    return 0;
}
