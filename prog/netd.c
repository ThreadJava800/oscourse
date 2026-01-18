#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <inc/error.h>
#include <inc/string.h>

int (*volatile net_read)(void *buf, size_t *size);
int (*volatile net_write)(void *buf, size_t size);
int (*volatile cprintf)(const char *fmt, ...);
void (*volatile packet_dump)(const uint8_t *const, const size_t);


#define ETH_MTU            1500
#define IP_HEADER_MIN_SIZE 20
#define UDP_HEADER_SIZE    8

#define ETH_TYPE_ARP  0x0806
#define ETH_TYPE_IPV4 0x0800

typedef uint8_t MacAddr[6];

#define GUEST_MAC_ADDR     {0x52, 0x54, 0x00, 0x12, 0x34, 0x57}
#define HOST_MAC_ADDR      {0x52, 0x54, 0x00, 0x12, 0x34, 0x56}
#define BROADCAST_MAC_ADDR {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}
MacAddr guest_mac_addr = GUEST_MAC_ADDR;
MacAddr host_mac_addr = HOST_MAC_ADDR;
MacAddr broadcast_mac_addr = BROADCAST_MAC_ADDR;

typedef struct {
    MacAddr dst_mac;
    MacAddr src_mac;
    uint16_t type;
} __attribute__((packed)) EthHdr;

#define IPV4_PROTOCOL_UDP 0x11

typedef struct {
    union {
        uint8_t addr_p[4];
        uint32_t addr;
    };
} __attribute__((packed)) Ipv4Addr;

#define HOST_IPV4_ADDR  {192, 168, 56, 1}
#define GUEST_IPV4_ADDR {192, 168, 56, 2}
Ipv4Addr host_ipv4_addr = (Ipv4Addr){.addr_p = HOST_IPV4_ADDR};
Ipv4Addr guest_ipv4_addr = (Ipv4Addr){.addr_p = GUEST_IPV4_ADDR};

typedef struct {
    MacAddr mac;
    Ipv4Addr ip;
} ArpMap;

#define MAX_ARP_TABLE_ELEM_CNT 4
ArpMap arp_table[MAX_ARP_TABLE_ELEM_CNT] = {
        {.mac = HOST_MAC_ADDR, .ip.addr_p = HOST_IPV4_ADDR}};

#define ARP_HARDWARE_TYPE_ETH  1
#define ARP_PROTOCOL_TYPE_IPV4 0x0800
#define ARP_HARDWARE_LEN_ETH   6
#define ARP_PROTOCOL_LEN_IPV4  4
#define ARP_OPCODE_REQUEST     1
#define ARP_OPCODE_RESPONSE    2

typedef struct {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_length;
    uint8_t protocol_length;
    uint16_t opcode;
    MacAddr sender_mac;
    Ipv4Addr sender_ipv4;
    MacAddr target_mac;
    Ipv4Addr target_ipv4;
} __attribute__((packed)) ArpMacIp4Hdr;

#define IPV4_HDR_LEN(x) (((x) & 0xF) * 4)
#define IPV4_VERSION(x) ((x) >> 4)

typedef struct {
    uint8_t version_hdr_len;
    uint8_t tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_frag_offset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    Ipv4Addr src_ip;
    Ipv4Addr dst_ip;
    uint8_t options[];
} __attribute__((packed)) Ipv4Hdr;

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} __attribute__((packed)) UdpHdr;

typedef struct {
    uint16_t port;
    Ipv4Addr addr;
} SockAddr;

uint16_t
ntoh16(uint16_t net_val) {
    return __builtin_bswap16(net_val);
}

uint16_t
hton16(uint16_t net_val) {
    return __builtin_bswap16(net_val);
}

void
init_arp_hdr(ArpMacIp4Hdr *hdr, uint8_t opcode) {
    hdr->hardware_type = hton16(ARP_HARDWARE_TYPE_ETH);
    hdr->protocol_type = hton16(ARP_PROTOCOL_TYPE_IPV4);
    hdr->hardware_length = ARP_HARDWARE_LEN_ETH;
    hdr->protocol_length = ARP_PROTOCOL_LEN_IPV4;
    hdr->opcode = hton16(opcode);
}

int
handle_arp(ArpMacIp4Hdr *arp_hdr, size_t length) {
    cprintf("%s: entry\n", __func__);

    if (ntoh16(arp_hdr->hardware_type) != ARP_HARDWARE_TYPE_ETH) {
        cprintf("%s: Unknown hardware type %u\n", __func__, arp_hdr->hardware_type);
        return -E_UNSUPPORTED;
    } else if (ntoh16(arp_hdr->protocol_type) != ARP_PROTOCOL_TYPE_IPV4) {
        cprintf("%s: Unknown protocol type %u\n", __func__, arp_hdr->protocol_type);
        return -E_UNSUPPORTED;
    } else if (arp_hdr->hardware_length != ARP_HARDWARE_LEN_ETH) {
        cprintf("%s: Invalid hardware length %u\n", __func__, arp_hdr->hardware_length);
        return -E_UNSUPPORTED;
    } else if (arp_hdr->protocol_length != ARP_PROTOCOL_LEN_IPV4) {
        cprintf("%s: Invalid protocol length %u\n", __func__, arp_hdr->protocol_length);
        return -E_UNSUPPORTED;
    }

    if (arp_hdr->target_ipv4.addr != guest_ipv4_addr.addr) {
        return 0;
    }

    ArpMacIp4Hdr arp_resp;
    init_arp_hdr(&arp_resp, ARP_OPCODE_RESPONSE);
    memcpy(&arp_resp.target_mac, &arp_hdr->sender_mac, sizeof(MacAddr));
    arp_resp.target_ipv4 = arp_hdr->sender_ipv4;
    memcpy(&arp_resp.sender_mac, &guest_mac_addr, sizeof(MacAddr));
    arp_resp.sender_ipv4 = guest_ipv4_addr;

    EthHdr eth_resp;
    memcpy(&eth_resp.dst_mac, &arp_hdr->sender_mac, sizeof(MacAddr));
    memcpy(&eth_resp.src_mac, &guest_mac_addr, sizeof(MacAddr));
    eth_resp.type = hton16(ETH_TYPE_ARP);

    struct {
        EthHdr eth;
        ArpMacIp4Hdr arp;
    } __attribute__((packed)) response = {
            .eth = eth_resp,
            .arp = arp_resp};

    net_write(&response, sizeof(response));

    return 0;
}

int
handle_udp(UdpHdr *hdr, size_t length) {
    cprintf("%s: entry\n", __func__);

    size_t hdr_len = ntoh16(hdr->length);
    if (hdr_len != length) {
        cprintf("%s: Invalid UDP packet length. "
                "Expected %lu, but length field is %lu\n",
                __func__,
                length, hdr_len);
        return -E_INVALID_PACKET;
    }

    uint8_t *data = (void *)hdr + sizeof(UdpHdr);
    size_t data_len = length - sizeof(UdpHdr);

    packet_dump(data, data_len);

    return 0;
}

int
handle_ipv4(Ipv4Hdr *hdr, size_t length) {
    cprintf("%s: entry\n", __func__);

    size_t hdr_len = IPV4_HDR_LEN(hdr->version_hdr_len);
    void *payload = (void *)hdr + hdr_len;
    size_t payload_length = length - hdr_len;

    switch (hdr->protocol) {
    case IPV4_PROTOCOL_UDP:
        return handle_udp(payload, payload_length);
    }
    return 0;
}

int
handle_packet(void *buffer, size_t length) {
    cprintf("%s: entry\n", __func__);

    EthHdr *eth_hdr = (EthHdr *)buffer;
    void *payload = buffer + sizeof(EthHdr);
    size_t payload_length = length - sizeof(EthHdr);

    switch (ntoh16(eth_hdr->type)) {
    case ETH_TYPE_ARP:
        return handle_arp(payload, payload_length);
    case ETH_TYPE_IPV4:
        return handle_ipv4(payload, payload_length);
    default:
        return -E_UNSUPPORTED;
    }
}

void
umain(int argc, char **argv) {
    for (;;) {
        uint8_t recv_buf[128];
        size_t recv_buf_size = sizeof(recv_buf);

        cprintf("Waiting for data on virtio-net...\n");

        int err = net_read(recv_buf, &recv_buf_size);
        if (err != 0) {
            cprintf("Failed to read from virtio-net with err = %d\n", err);
            break;
        }

        cprintf("Received from virtio-net:\n");
        packet_dump(recv_buf, recv_buf_size);
        handle_packet(recv_buf, recv_buf_size);
        cprintf("\n");
    }
}