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
    if (pci_get_bar_type(pci_dev, VIRTIO_PCI_IO_BAR_INDEX) != PciBarPMIO) {
        cprintf("%s: Only legacy devices are supported\n", __func__);
        return -E_UNSUPPORTED;
    }

    return 0;
}

#define IMPL_VIRTIO_WRITE_FUN(bitsize)                                     \
    void virtio_write##bitsize(PciDevice *pci_dev,                        \
                               uint8_t offset, uint##bitsize##_t value) { \
        assert(pci_dev != NULL);                                          \
        pci_access_write##bitsize(pci_dev, VIRTIO_PCI_IO_BAR_INDEX,       \
                                  offset, value);                         \
    }

IMPL_VIRTIO_WRITE_FUN(8)
IMPL_VIRTIO_WRITE_FUN(16)
IMPL_VIRTIO_WRITE_FUN(32)
IMPL_VIRTIO_WRITE_FUN(64)

#undef IMPL_VIRTIO_WRITE_FUN

#define IMPL_VIRTIO_READ_FUN(bitsize)                                      \
    uint##bitsize##_t virtio_read##bitsize(PciDevice *pci_dev,            \
                                           uint8_t offset) {              \
        assert(pci_dev != NULL);                                          \
        return pci_access_read##bitsize(pci_dev, VIRTIO_PCI_IO_BAR_INDEX, \
                                        offset);                          \
    }

IMPL_VIRTIO_READ_FUN(8)
IMPL_VIRTIO_READ_FUN(16)
IMPL_VIRTIO_READ_FUN(32)
IMPL_VIRTIO_READ_FUN(64)

#undef IMPL_VIRTIO_READ_FUN

void
virtio_set_queue(VirtioDevice *virtio_dev, Virtq *queue) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    uint64_t div_val = (uint64_t)queue->descriptor_table / VIRTQ_ALIGNMENT;
    assert(div_val < UINT32_MAX);
    virtio_write32(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_ADDRESS,
                   (uint32_t)div_val);
}

void
virtio_select_queue(VirtioDevice *virtio_dev, uint16_t index) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    virtio_write16(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_SELECT,
                   index);
}

uint16_t
virtio_read_queue_size(VirtioDevice *virtio_dev) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    return virtio_read16(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_SIZE);
}

void
virtio_reset(VirtioDevice *virtio_dev) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    virtio_write8(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_DEVICE_STATUS,
                  0);
}

void
virtio_set_status(VirtioDevice *virtio_dev, uint8_t status) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    virtio_write8(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_DEVICE_STATUS,
                  status);
}

uint32_t
virtio_read_device_features(VirtioDevice *virtio_dev) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    return virtio_read32(pci_dev, VIRTIO_PCI_OFFSET_DEVICE_FEATURES);
}

void
virtio_set_driver_features(VirtioDevice *virtio_dev, uint32_t features) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    virtio_write32(pci_dev, VIRTIO_PCI_OFFSET_GUEST_FEATURES,
                   features);
}

uint8_t
virtio_read_isr(VirtioDevice *virtio_dev) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    return virtio_read8(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_DEVICE_ISR);
}

uint16_t
virtio_read_notify(VirtioDevice *virtio_dev) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    return virtio_read16(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_NOTIFY);
}

void
virtio_notify(VirtioDevice *virtio_dev, uint16_t id) {
    assert(virtio_dev != NULL);

    PciDevice *pci_dev = virtio_dev->pci_dev;
    assert(pci_dev != NULL);

    virtio_write8(pci_dev, VIRTIO_PCI_OFFSET_QUEUE_NOTIFY, id);
}

static
int
virtq_init(Virtq *virtq, uint16_t idx, void *buffer, size_t buffer_size, size_t queue_size) {
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
    virtq->idx = idx;

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

    virtq->head_free_desc = 0;
    virtq->cnt_free_desc = queue_size;
    virtq->last_seen_used_desc = 0;

    return 0;
}

static int
virtq_place_buffer(Virtq *virtq, void *buffer, uint32_t length, bool is_writable, uint16_t *head) {
    assert(virtq != NULL);
    assert(buffer != NULL);
    assert(length != 0);
    assert(head != NULL);

    if (virtq->cnt_free_desc == 0) {
        cprintf("%s: No free descriptors\n", __func__);
        return -E_NO_MEM;
    }

    uint16_t free_desc = virtq->head_free_desc;
    *head = free_desc;

    virtq->descriptor_table[free_desc].address = (uint64_t)buffer;
    virtq->descriptor_table[free_desc].length = length;
    /* no support for chaining */
    virtq->descriptor_table[free_desc].flags = is_writable ? VIRTQ_DESC_F_WRITE : 0;

    virtq->head_free_desc = (free_desc + 1) % virtq->queue_size;
    --virtq->cnt_free_desc;

    return 0;
}

int
virtio_send_buffer(VirtioDevice *virtio_dev, Virtq *virtq, void *buffer, uint32_t length, bool is_writable) {
    assert(virtq != NULL);
    assert(buffer != NULL);
    assert(length != 0);

    uint16_t head = 0;
    int err = virtq_place_buffer(virtq, buffer, length, is_writable, &head);
    if (err != 0) {
        return err;
    }

    VirtqAvailable avail = virtq->available_ring;
    avail.ring[*avail.idx % virtq->queue_size] = head;
    ++(*avail.idx);

    virtio_notify(virtio_dev, virtq->idx);
    return 0;
}

int
virtio_recv_buffer(VirtioDevice *virtio_dev, Virtq *virtq, recv_handler_t handler) {
    assert(virtio_dev != NULL);
    assert(virtq != NULL);

    if (virtq->last_seen_used_desc == *virtq->used_ring.idx) {
        return 0;
    }

    uint16_t ring_idx = virtq->last_seen_used_desc % virtq->queue_size;
    VirtqUsedElem *used_elem = &virtq->used_ring.ring[ring_idx];
    assert("Chaining is not supported" && used_elem->len == 1);

    int res = handler((void *)virtq->descriptor_table[used_elem->id].address);
    ++virtq->last_seen_used_desc;

    return res;
}

int
virtio_setup_queue(VirtioDevice *virtio_dev, Virtq *virtq, uint16_t idx, void *buffer, size_t buffer_size) {
    assert(virtio_dev != NULL);
    assert(virtq != NULL);
    assert(buffer != NULL);
    assert(buffer_size != 0);

    virtio_select_queue(virtio_dev, idx);

    uint16_t queue_size = virtio_read_queue_size(virtio_dev);
    int err = virtq_init(virtq, idx, buffer, buffer_size, queue_size);
    if (err != 0) {
        cprintf("%s: Unable to initialize virtq structure\n", __func__);
        return err;
    }

    virtio_set_queue(virtio_dev, buffer);

    return 0;
}
