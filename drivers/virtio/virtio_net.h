/**
    Definitions for virtio network card device.
    Adopted from OVMF.
**/
#ifndef JOS_DRIVERS_VIRTIO_VIRTIO_NET_H
#define JOS_DRIVERS_VIRTIO_VIRTIO_NET_H

#include <drivers/virtio/virtio_base.h>
#include <stdalign.h>

#define VIRTIO_DEVICE_ID_NETWORK 1

//
// virtio-0.9.5, Appendix C: Network Device
//
#pragma pack(1)
typedef struct {
    uint8_t mac[6];
    uint16_t link_status;
} VirtioNetConfig;
#pragma pack()

#define OFFSET_OF_VNET(Field) offsetof(VIRTIO_NET_CONFIG, Field)
#define SIZE_OF_VNET(Field)   (sizeof((VIRTIO_NET_CONFIG *)0)->Field)

//
// Queue Identifiers
//
#define VIRTIO_NET_Q_RX 0
#define VIRTIO_NET_Q_TX 1

//
// Feature Bits
//
#define VIRTIO_NET_F_CSUM           (1u << 0)  // host to checksum outgoing packets
#define VIRTIO_NET_F_GUEST_CSUM     (1u << 1)  // guest to checksum incoming packets
#define VIRTIO_NET_F_MAC            (1u << 5)  // MAC available to guest
#define VIRTIO_NET_F_GSO            (1u << 6)  // deprecated
#define VIRTIO_NET_F_GUEST_TSO4     (1u << 7)  // guest can receive TSOv4
#define VIRTIO_NET_F_GUEST_TSO6     (1u << 8)  // guest can receive TSOv6
#define VIRTIO_NET_F_GUEST_ECN      (1u << 9)  // guest can receive TSO with ECN
#define VIRTIO_NET_F_GUEST_UFO      (1u << 10) // guest can receive UFO
#define VIRTIO_NET_F_HOST_TSO4      (1u << 11) // host can receive TSOv4
#define VIRTIO_NET_F_HOST_TSO6      (1u << 12) // host can receive TSOv6
#define VIRTIO_NET_F_HOST_ECN       (1u << 13) // host can receive TSO with ECN
#define VIRTIO_NET_F_HOST_UFO       (1u << 14) // host can receive UFO
#define VIRTIO_NET_F_MRG_RXBUF      (1u << 15) // guest can merge receive buffers
#define VIRTIO_NET_F_STATUS         (1u << 16) // link status available to guest
#define VIRTIO_NET_F_CTRL_VQ        (1u << 17) // control channel available
#define VIRTIO_NET_F_CTRL_RX        (1u << 18) // control channel RX mode support
#define VIRTIO_NET_F_CTRL_VLAN      (1u << 19) // control channel VLAN filtering
#define VIRTIO_NET_F_GUEST_ANNOUNCE (1u << 21) // guest can send gratuitous pkts

//
// Packet Header
//
#pragma pack(1)
typedef struct {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
} VirtioNetReq;
#pragma pack()

//
// Bits in VIRTIO_NET_REQ.Flags
//
#define VIRTIO_NET_HDR_F_NEEDS_CSUM (1u << 0)

//
// Types/Bits for VIRTIO_NET_REQ.GsoType
//
#define VIRTIO_NET_HDR_GSO_NONE  0x00
#define VIRTIO_NET_HDR_GSO_TCPV4 0x01
#define VIRTIO_NET_HDR_GSO_UDP   0x03
#define VIRTIO_NET_HDR_GSO_TCPV6 0x04
#define VIRTIO_NET_HDR_GSO_ECN   (1u << 7)

//
// Link Status Bits in VIRTIO_NET_CONFIG.LinkStatus
//
#define VIRTIO_NET_S_LINK_UP  (1u << 0)
#define VIRTIO_NET_S_ANNOUNCE (1u << 1)

#define QUEUE_BUFFER_SIZE (2 * VIRTQ_ALIGNMENT)

typedef struct {
    VirtioDevice virtio_dev;
    VirtioNetConfig net_config;
    Virtq rx_virtq;
    Virtq tx_virtq;
    alignas(VIRTQ_ALIGNMENT) uint8_t rx_buffer[QUEUE_BUFFER_SIZE];
    alignas(VIRTQ_ALIGNMENT) uint8_t tx_buffer[QUEUE_BUFFER_SIZE];
} VirtioNetDevice;

#define REQUIRED_FEATURES (VIRTIO_NET_F_MAC | VIRTIO_NET_F_STATUS)

int virtio_net_init(VirtioNetDevice *virtio_net_dev, PciDevice *pci_dev);

#endif // JOS_DRIVERS_VIRTIO_VIRTIO_NET_H