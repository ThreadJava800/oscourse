/**
    Legacy Virtio (0.9.5) header file.
    Adopted from OVMF.
**/
#ifndef JOS_DRIVERS_VIRTIO_VIRTIO_H
#define JOS_DRIVERS_VIRTIO_VIRTIO_H

#include <inc/types.h>
#include <drivers/pci/pci.h>

#define VIRTIO_VENDOR_ID       0x1AF4
#define VIRTIO_DEVICE_ID_START 0x1040
#define VIRTIO_DEVICE_ID_END   0x107f

//
// VirtIo Device Specific Configuration Offsets
//
#define VIRTIO_DEVICE_SPECIFIC_CONFIGURATION_OFFSET_PCI 20

//
// PCI VirtIo Header Offsets
//
#define VIRTIO_PCI_OFFSET_DEVICE_FEATURES     0x00
#define VIRTIO_PCI_OFFSET_GUEST_FEATURES      0x04
#define VIRTIO_PCI_OFFSET_QUEUE_ADDRESS       0x08
#define VIRTIO_PCI_OFFSET_QUEUE_SIZE          0x0C
#define VIRTIO_PCI_OFFSET_QUEUE_SELECT        0x0E
#define VIRTIO_PCI_OFFSET_QUEUE_NOTIFY        0x10
#define VIRTIO_PCI_OFFSET_QUEUE_DEVICE_STATUS 0x12
#define VIRTIO_PCI_OFFSET_QUEUE_DEVICE_ISR    0x13

#define VIRTIO_PCI_DEVICE_STATUS_RESET_VAL 0

#define VIRTIO_PCI_IO_BAR_INDEX 0

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT 1
/* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTQ_DESC_F_WRITE 2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT 4
#define VIRTQ_DESC_ELEM_SIZE  sizeof(VirtqDescriptor)

typedef struct {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
} VirtqDescriptor;

#define VIRTQ_AVAIL_F_NO_INTERRUPT 1
#define VIRTQ_AVAIL_ELEM_SIZE      2
#define VIRTQ_AVAIL_FLAGS_OFFSET   0
#define VIRTQ_AVAIL_IDX_OFFSET     2
#define VIRTQ_AVAIL_RING_OFFSET    4
#define VIRTQ_AVAIL_EVENT_OFFSET(queue_size) \
    (VIRTQ_AVAIL_RING_OFFSET + (VIRTQ_AVAIL_ELEM_SIZE * (queue_size)))
#define VIRTQ_AVAIL_END_OFFSET(queue_size) \
    (VIRTQ_AVAIL_EVENT_OFFSET(queue_size) + 2)

typedef struct {
    uint16_t *flags;
    uint16_t *idx;
    uint16_t *ring;
    uint16_t *avail_event;
} VirtqAvailable;

typedef struct {
    uint32_t id;
    uint32_t len;
} VirtqUsedElem;

#define VIRTQ_USED_ELEM_SIZE    8
#define VIRTQ_USED_FLAGS_OFFSET 0
#define VIRTQ_USED_IDX_OFFSET   2
#define VIRTQ_USED_RING_OFFSET  4
#define VIRTQ_USED_EVENT_OFFSET(queue_size) \
    (VIRTQ_USED_RING_OFFSET + (VIRTQ_USED_ELEM_SIZE * (queue_size)))
#define VIRTQ_USED_END_OFFSET(queue_size) \
    (VIRTQ_USED_EVENT_OFFSET(queue_size) + 2)

typedef struct {
    uint16_t *flags;
    uint16_t *idx;
    VirtqUsedElem *ring;
    uint16_t *used_event;
} VirtqUsed;

#define VIRTQ_ALIGNMENT 4096

typedef struct {
    VirtqDescriptor *descriptor_table;
    VirtqAvailable available_ring;
    VirtqUsed used_ring;
    uint16_t queue_size;
    void *buffer;
    size_t buffer_size;
} Virtq;

//
// virtio-0.9.5, 2.2.2.1 Device Status
//
#define VSTAT_ACK       BIT0
#define VSTAT_DRIVER    BIT1
#define VSTAT_DRIVER_OK BIT2
#define VSTAT_FAILED    BIT7

//
// virtio-0.9.5, Appendix B: Reserved (Device-Independent) Feature Bits
//
#define VIRTIO_F_NOTIFY_ON_EMPTY    BIT24
#define VIRTIO_F_RING_INDIRECT_DESC BIT28
#define VIRTIO_F_RING_EVENT_IDX     BIT29

typedef struct {
    PciDevice *pci_dev;
    uint16_t id;
} VirtioDevice;

bool virtio_check(PciDevice *dev, uint16_t *id);
int virtio_init(VirtioDevice *virtio_dev, PciDevice *pci_dev);

void virtio_set_queue(VirtioDevice *virtio_dev, Virtq *queue);
void virtio_select_queue(VirtioDevice *virtio_dev, uint16_t index);
uint16_t virtio_read_queue_size(VirtioDevice *virtio_dev);

void virtio_reset(VirtioDevice *virtio_dev);
void virtio_set_status(VirtioDevice *virtio_dev, uint8_t status);

uint32_t virtio_read_device_features(VirtioDevice *virtio_dev);
void virtio_set_driver_features(VirtioDevice *virtio_dev, uint32_t features);

uint8_t virtio_read_isr(VirtioDevice *virtio_dev);
uint16_t virtio_read_notify(VirtioDevice *virtio_dev);
void virtio_notify(VirtioDevice *virtio_dev, uint16_t id);

#endif // JOS_DRIVERS_VIRTIO_VIRTIO_H
