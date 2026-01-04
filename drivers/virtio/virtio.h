/**
    Virtio 1.0 header file.
    Adopted from OVMF.
**/
#ifndef JOS_DRIVERS_VIRTIO_VIRTIO_H
#define JOS_DRIVERS_VIRTIO_VIRTIO_H

#include <inc/types.h>
#include <drivers/pci/pci.h>
#include <stdalign.h>

#define VIRTIO_VENDOR_ID       0x1AF4
#define VIRTIO_DEVICE_ID_START 0x1040
#define VIRTIO_DEVICE_ID_END   0x107f

#define VIRTIO_DEVICE_ID_NETWORK_CARD 1

//
// Structures for parsing the VirtIo 1.0 specific PCI capabilities from the
// config space
//
#pragma pack(push)
typedef struct {
    PciCapabilityHdr vendor_hdr;
    uint8_t config_type; // Identifies the specific VirtIo 1.0 config structure
    uint8_t bar;         // The BAR that contains the structure
    uint8_t padding[3];
    uint32_t offset; // Offset within Bar until the start of the structure
    uint32_t length; // Length of the structure
} VirtioPciCapability;
#pragma pack(pop)

//
// Values for the VirtioPciCapability.config_type field
//
#define VIRTIO_PCI_CAP_COMMON_CFG 1 // Common configuration
#define VIRTIO_PCI_CAP_NOTIFY_CFG 2 // Notifications
#define VIRTIO_PCI_CAP_DEVICE_CFG 4 // Device specific configuration

//
// Structure pointed-to by Bar and Offset in VIRTIO_PCI_CAP when ConfigType is
// VIRTIO_PCI_CAP_COMMON_CFG
//
#pragma pack(push)
typedef struct {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t msix_config;
    uint16_t num_queues;
    uint8_t device_status;
    uint8_t config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_avail;
    uint64_t queue_used;
} VirtioPciCommonConfig;
#pragma pack(pop)

//
// VirtIo 1.0 device status bits
//
#define VSTAT_FEATURES_OK (1u << 3)

//
// VirtIo 1.0 reserved (device-independent) feature bits
//
#define VIRTIO_F_VERSION_1 (1u << 32)

/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT 1
/* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTQ_DESC_F_WRITE 2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT 4
#define VIRTQ_DESC_ELEM_SIZE  sizeof(VirtqDescriptor)
#define VIRTQ_DESC_ALIGNMENT  16

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
#define VIRTQ_USED_ALIGNMENT    4
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

int
virtq_init(Virtq *virtq, void *buffer, size_t buffer_size, size_t queue_size);

#endif // JOS_DRIVERS_VIRTIO_VIRTIO_H
