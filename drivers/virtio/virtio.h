#ifndef JOS_DRIVERS_VIRTIO_VIRTIO_H
#define JOS_DRIVERS_VIRTIO_VIRTIO_H

#include "virtio_net.h"

int virtio_module_init();

VirtioNetDevice *get_virtio_net_dev();

#endif // JOS_DRIVERS_VIRTIO_VIRTIO_H