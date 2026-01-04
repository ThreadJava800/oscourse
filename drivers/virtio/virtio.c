#include <drivers/virtio/virtio.h>
#include <inc/assert.h>
#include <inc/error.h>
#include <inc/stdio.h>

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
virtio_init(VirtioDevice *virtio_dev, PciDevice *pci_dev) {
    assert(virtio_dev != NULL);
    assert(pci_dev != NULL);

    *virtio_dev = (VirtioDevice){};
    if (!virtio_check(pci_dev, &virtio_dev->id)) {
        cprintf("%s: PCI device %p isn't virtio device\n", __func__, pci_dev);
        return -E_UNSUPPORTED;
    }

    virtio_dev->pci_dev = pci_dev;
    if (pci_get_bar_type(pci_dev, VIRTIO_PCI_BAR_INDEX) != PciBarPMIO) {
        cprintf("%s: Only legacy devices are supported\n", __func__);
        return -E_UNSUPPORTED;
    }

    return 0;
}

#define DEF_VIRTIO_WRITE_FUN(bitsize)                                     \
    void virtio_write##bitsize(PciDevice *pci_dev,                        \
                               uint8_t offset, uint##bitsize##_t value) { \
        assert(pci_dev != NULL);                                          \
        pci_access_write##bitsize(pci_dev, VIRTIO_PCI_BAR_INDEX,          \
                                  offset, value);                         \
    }

DEF_VIRTIO_WRITE_FUN(8);
DEF_VIRTIO_WRITE_FUN(16);
DEF_VIRTIO_WRITE_FUN(32);
DEF_VIRTIO_WRITE_FUN(64);

void
virtio_set_queue(VirtioDevice *virtio_dev, Virtq *queue) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    virtio_write64(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_ADDRESS,
                   (uint64_t)queue->descriptor_table);
}

void
virtio_select_queue(VirtioDevice *virtio_dev, uint16_t index) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    virtio_write16(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_SELECT,
                   index);
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
    used_offset = (used_offset + VIRTQ_ALIGNMENT) & VIRTQ_ALIGNMENT;

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