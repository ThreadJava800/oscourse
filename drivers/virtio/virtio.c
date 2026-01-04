#include "drivers/pci/pci.h"
#include "inc/error.h"
#include "inc/stdio.h"
#include <drivers/virtio/virtio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

bool
virtio_check(PciDevice *dev, uint16_t *id) {
    uint16_t vid = pci_get_vid(dev);
    uint16_t did = pci_get_did(dev);

    if (vid == VIRTIO_VENDOR_ID &&
        did >= VIRTIO_DEVICE_ID_START &&
        did <= VIRTIO_DEVICE_ID_END) {
        if (id != NULL) {
            *id = did - VIRTIO_DEVICE_ID_START;
        }
        return true;
    }

    return false;
}

int
virtq_init(Virtq *virtq, void *buffer, size_t buffer_size, size_t queue_size) {
    if ((uintptr_t)buffer != ((uintptr_t)buffer & VIRTQ_ALIGNMENT)) {
        cprintf("%s: Invalid buffer alignment\n", __func__);
        return -E_INVAL;
    }

    if ((buffer_size & (buffer_size - 1)) != 0) {
        cprintf("%s: Buffer size is not power of two\n", __func__);
        return -E_INVAL;
    }

    virtq->buffer = buffer;
    virtq->buffer_size = buffer_size;
    virtq->descriptor_table = buffer;
    virtq->queue_size = queue_size;

    size_t avail_offset = VIRTQ_DESC_ELEM_SIZE * queue_size;
    void *curr_ptr = buffer + avail_offset;
    virtq->available_ring.flags = curr_ptr + VIRTQ_AVAIL_FLAGS_OFFSET;
    virtq->available_ring.idx = curr_ptr + VIRTQ_AVAIL_IDX_OFFSET;
    virtq->available_ring.ring = curr_ptr + VIRTQ_AVAIL_RING_OFFSET;

    curr_ptr = buffer + avail_offset + VIRTQ_AVAIL_EVENT_OFFSET(queue_size);
    virtq->available_ring.idx = curr_ptr;

    size_t used_offset = avail_offset + VIRTQ_AVAIL_END_OFFSET(queue_size);
    used_offset = ((used_offset - 1) & (~VIRTQ_USED_ALIGNMENT)) + VIRTQ_USED_ALIGNMENT;
    curr_ptr = buffer + used_offset;
    virtq->used_ring.flags = curr_ptr + VIRTQ_USED_FLAGS_OFFSET;
    virtq->used_ring.idx = curr_ptr + VIRTQ_USED_IDX_OFFSET;
    virtq->used_ring.ring = curr_ptr + VIRTQ_USED_RING_OFFSET;

    curr_ptr = buffer + used_offset + VIRTQ_USED_EVENT_OFFSET(queue_size);
    virtq->used_ring.used_event = curr_ptr;

    size_t total_size = used_offset + VIRTQ_USED_END_OFFSET(queue_size);
    if (total_size > buffer_size) {
        cprintf("%s: Buffer size isn't enough for queue structure\n", __func__);
        *virtq = (Virtq){};
        return -E_INVAL;
    }

    return 0;
}