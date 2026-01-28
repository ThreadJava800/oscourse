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

#define IPV4_PROTOCOL_TCP 0x06
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

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_PSH 0x08
#define TCP_FLAG_ACK 0x10
#define TCP_FLAG_URG 0x20

#define TCP_STATE_CLOSED      0
#define TCP_STATE_LISTEN      1
#define TCP_STATE_SYN_SENT    2
#define TCP_STATE_SYN_RCVD    3
#define TCP_STATE_ESTABLISHED 4
#define TCP_STATE_FIN_WAIT_1  5
#define TCP_STATE_FIN_WAIT_2  6
#define TCP_STATE_CLOSE_WAIT  7
#define TCP_STATE_CLOSING     8
#define TCP_STATE_LAST_ACK    9
#define TCP_STATE_TIME_WAIT   10

#define TCP_DEFAULT_MSS 1460
#define TCP_DEFAULT_WINDOW 65535
#define TCP_MAX_CONNECTIONS 10

typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;  /* 4 bits */
    uint8_t  flags;        /* 6 bits */
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
    uint8_t  options[];
} __attribute__((packed)) TcpHdr;

typedef struct TcpConnection {
    uint32_t    state;
    uint32_t    local_ip;
    uint32_t    remote_ip;
    uint16_t    local_port;
    uint16_t    remote_port;
    
    uint32_t    snd_una;
    uint32_t    snd_nxt;
    uint32_t    snd_wnd;
    
    uint32_t    rcv_nxt;
    uint32_t    rcv_wnd;
    
    uint8_t     recv_buf[4096];
    uint32_t    recv_buf_len;
    uint32_t    recv_buf_read;
    
    uint8_t     send_buf[4096];
    uint32_t    send_buf_len;
    uint32_t    send_buf_sent;
    
    uint32_t    retransmit_timeout;
    uint32_t    last_ack_time;
    
    uint32_t    cwnd;
    uint32_t    ssthresh;
    
    struct TcpConnection *next;
} TcpConnection;

static TcpConnection tcp_connection_pool[TCP_MAX_CONNECTIONS];
static bool tcp_connection_used[TCP_MAX_CONNECTIONS];
TcpConnection *tcp_connections = NULL;
int handle_tcp(TcpHdr *hdr, size_t length, Ipv4Addr src_ip, Ipv4Addr dst_ip);
TcpConnection* tcp_find_connection(uint32_t src_ip, uint16_t src_port,
                                   uint32_t dst_ip, uint16_t dst_port);
TcpConnection* tcp_create_connection(uint32_t local_ip, uint16_t local_port,
                                     uint32_t remote_ip, uint16_t remote_port);
void tcp_free_connection(TcpConnection *conn);
void send_tcp_packet(TcpConnection *conn, uint8_t flags, 
                     uint8_t *data, uint16_t data_len);
int handle_tcp_syn(TcpConnection *conn, TcpHdr *tcp_hdr, uint32_t seq, uint32_t src_ip_val);
int handle_tcp_syn_ack(TcpConnection *conn, TcpHdr *tcp_hdr, uint32_t seq, uint32_t ack);
int handle_tcp_ack(TcpConnection *conn, TcpHdr *tcp_hdr, uint32_t ack, 
                   uint8_t *payload, uint16_t payload_len);
int handle_tcp_fin(TcpConnection *conn);

uint16_t
ntoh16(uint16_t net_val) {
    return __builtin_bswap16(net_val);
}

uint16_t
hton16(uint16_t net_val) {
    return __builtin_bswap16(net_val);
}

uint32_t
ntoh32(uint32_t net_val) {
    return __builtin_bswap32(net_val);
}

uint32_t
hton32(uint32_t net_val) {
    return __builtin_bswap32(net_val);
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
    case IPV4_PROTOCOL_TCP:
        return handle_tcp(payload, payload_length, hdr->src_ip, hdr->dst_ip);
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

static inline uint16_t
tcp_header_length(TcpHdr *hdr) {
    return (hdr->data_offset >> 4) * 4;
}

TcpConnection*
tcp_find_connection(uint32_t src_ip, uint16_t src_port,
                    uint32_t dst_ip, uint16_t dst_port) {
    TcpConnection *conn = tcp_connections;
    while (conn) {
        if (conn->local_ip == dst_ip && conn->local_port == dst_port &&
            conn->remote_ip == src_ip && conn->remote_port == src_port) {
            return conn;
        }
        conn = conn->next;
    }
    return NULL;
}

TcpConnection*
tcp_create_connection(uint32_t local_ip, uint16_t local_port,
                      uint32_t remote_ip, uint16_t remote_port) {
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (!tcp_connection_used[i]) {
            TcpConnection *conn = &tcp_connection_pool[i];
            memset(conn, 0, sizeof(TcpConnection));
            
            conn->state = TCP_STATE_CLOSED;
            conn->local_ip = local_ip;
            conn->local_port = local_port;
            conn->remote_ip = remote_ip;
            conn->remote_port = remote_port;
            conn->rcv_wnd = TCP_DEFAULT_WINDOW;
            conn->snd_wnd = TCP_DEFAULT_WINDOW;
            conn->cwnd = 1;
            conn->ssthresh = TCP_DEFAULT_WINDOW;
            conn->snd_nxt = 1000 + (i * 1000);
            conn->next = tcp_connections;
            tcp_connections = conn;
            
            tcp_connection_used[i] = true;
            cprintf("Created TCP connection at slot %d\n", i);
            return conn;
        }
    }
    cprintf("No free TCP connection slots\n");
    return NULL;
}

void
tcp_free_connection(TcpConnection *conn) {
    int index = -1;
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        if (&tcp_connection_pool[i] == conn) {
            index = i;
            break;
        }
    }
    
    if (index >= 0) {
        TcpConnection **prev = &tcp_connections;
        while (*prev) {
            if (*prev == conn) {
                *prev = conn->next;
                break;
            }
            prev = &((*prev)->next);
        }
        tcp_connection_used[index] = false;
        cprintf("Freed TCP connection slot %d\n", index);
    }
}

void
send_tcp_packet(TcpConnection *conn, uint8_t flags, 
                     uint8_t *data, uint16_t data_len) {
    uint8_t packet[ETH_MTU];
    EthHdr *eth = (EthHdr*)packet;
    Ipv4Hdr *ip = (Ipv4Hdr*)(eth + 1);
    TcpHdr *tcp = (TcpHdr*)(ip + 1);
    uint8_t *payload = (uint8_t*)(tcp + 1);
    bool found_mac = false;
    for (int i = 0; i < MAX_ARP_TABLE_ELEM_CNT; i++) {
        if (arp_table[i].ip.addr == conn->remote_ip) {
            memcpy(eth->dst_mac, arp_table[i].mac, sizeof(MacAddr));
            found_mac = true;
            break;
        }
    }
    
    if (!found_mac) {
        memcpy(eth->dst_mac, broadcast_mac_addr, sizeof(MacAddr));
        cprintf("Warning: MAC address for IP %08x not found, using broadcast\n", conn->remote_ip);
    }
    
    memcpy(eth->src_mac, guest_mac_addr, sizeof(MacAddr));
    eth->type = hton16(ETH_TYPE_IPV4);
    ip->version_hdr_len = 0x45;
    ip->tos = 0;
    ip->total_length = hton16(sizeof(Ipv4Hdr) + sizeof(TcpHdr) + data_len);
    ip->identification = hton16(0);
    ip->flags_frag_offset = 0;
    ip->ttl = 64;
    ip->protocol = IPV4_PROTOCOL_TCP;
    ip->header_checksum = 0;
    ip->src_ip.addr = conn->local_ip;
    ip->dst_ip.addr = conn->remote_ip;
    tcp->src_port = hton16(conn->local_port);
    tcp->dst_port = hton16(conn->remote_port);
    tcp->seq_num = hton32(conn->snd_nxt);
    tcp->ack_num = hton32(conn->rcv_nxt);
    tcp->data_offset = (sizeof(TcpHdr) / 4) << 4;
    tcp->flags = flags;
    tcp->window = hton16(conn->rcv_wnd);
    tcp->checksum = 0;
    tcp->urgent_ptr = 0;
    if (data && data_len > 0) {
        if (data_len <= ETH_MTU - sizeof(EthHdr) - sizeof(Ipv4Hdr) - sizeof(TcpHdr)) {
            memcpy(payload, data, data_len);
            conn->snd_nxt += data_len;
        } else {
            cprintf("Warning: Data too large for TCP packet\n");
            data_len = 0;
        }
    }
    if ((flags & TCP_FLAG_SYN) || (flags & TCP_FLAG_FIN)) {
        conn->snd_nxt += 1;
    }
    size_t total_len = sizeof(EthHdr) + sizeof(Ipv4Hdr) + sizeof(TcpHdr) + data_len;
    net_write(packet, total_len);
    
    cprintf("Sent TCP packet: flags=0x%x seq=%u ack=%u len=%u\n", 
            flags, ntoh32(tcp->seq_num), ntoh32(tcp->ack_num), data_len);
}

int
handle_tcp_syn(TcpConnection *conn, TcpHdr *tcp_hdr, uint32_t seq, uint32_t src_ip_val) {
    if (conn->state == TCP_STATE_LISTEN) {
        if (conn->remote_ip == 0 && conn->remote_port == 0) {
            conn->state = TCP_STATE_SYN_RCVD;
            conn->remote_ip = src_ip_val;
            conn->remote_port = ntoh16(tcp_hdr->src_port);
            conn->rcv_nxt = seq + 1;
            if (conn->snd_nxt < 1000) {
                conn->snd_nxt = 2000;
            }
            send_tcp_packet(conn, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0);
            cprintf("TCP: Sent SYN-ACK for new connection\n");
            return 0;
        } else {
            cprintf("TCP: Connection already initialized\n");
            return -E_INVALID_EXE;
        }
    }
    cprintf("TCP: Not in LISTEN state for SYN (state=%d)\n", conn->state);
    return -E_UNSUPPORTED;
}

int
handle_tcp_syn_ack(TcpConnection *conn, TcpHdr *tcp_hdr, uint32_t seq, uint32_t ack) {
    if (conn->state == TCP_STATE_SYN_SENT) {
        if (ack == conn->snd_una + 1) {
            conn->state = TCP_STATE_ESTABLISHED;
            conn->rcv_nxt = seq + 1;
            conn->snd_una = ack;
            send_tcp_packet(conn, TCP_FLAG_ACK, NULL, 0);
            cprintf("TCP connection established\n");
            return 0;
        } else {
            cprintf("TCP: Invalid ACK number: %u (expected %u)\n", ack, conn->snd_una + 1);
        }
    } else {
        cprintf("TCP: Not in SYN_SENT state for SYN-ACK (state=%d)\n", conn->state);
    }
    return -E_INVALID_PACKET;
}

int
handle_tcp_ack(TcpConnection *conn, TcpHdr *tcp_hdr, uint32_t ack, 
                   uint8_t *payload, uint16_t payload_len) {
    if (conn->state == TCP_STATE_ESTABLISHED || 
        conn->state == TCP_STATE_CLOSE_WAIT ||
        conn->state == TCP_STATE_FIN_WAIT_1 ||
        conn->state == TCP_STATE_FIN_WAIT_2 ||
        conn->state == TCP_STATE_CLOSING ||
        conn->state == TCP_STATE_LAST_ACK) {
        if (ack > conn->snd_una) {
            conn->snd_una = ack;
        }
        if (payload_len > 0) {
            uint32_t seq = ntoh32(tcp_hdr->seq_num);
            if (seq == conn->rcv_nxt) {
                uint32_t free_space = sizeof(conn->recv_buf) - conn->recv_buf_len;
                if (payload_len <= free_space) {
                    memcpy(conn->recv_buf + conn->recv_buf_len, payload, payload_len);
                    conn->recv_buf_len += payload_len;
                    conn->rcv_nxt += payload_len;
                    cprintf("TCP: Received %u bytes, total buffered: %u\n", 
                           payload_len, conn->recv_buf_len);
                } else {
                    cprintf("TCP: Receive buffer full (%u/%u)\n", 
                           conn->recv_buf_len, sizeof(conn->recv_buf));
                }
                send_tcp_packet(conn, TCP_FLAG_ACK, NULL, 0);
            } else {
                cprintf("TCP: Unexpected sequence: %u (expected %u)\n", seq, conn->rcv_nxt);
            }
        } else if (ack > conn->snd_una) {
            send_tcp_packet(conn, TCP_FLAG_ACK, NULL, 0);
        }
        
        return 0;
    }
    
    cprintf("TCP: ACK received in invalid state: %d\n", conn->state);
    return -E_UNSUPPORTED;
}

int
handle_tcp_fin(TcpConnection *conn) {
    switch (conn->state) {
        case TCP_STATE_ESTABLISHED:
            conn->state = TCP_STATE_CLOSE_WAIT;
            conn->rcv_nxt += 1;
            send_tcp_packet(conn, TCP_FLAG_ACK, NULL, 0);
            cprintf("TCP: Received FIN, sent ACK, state=CLOSE_WAIT\n");
            break;
            
        case TCP_STATE_FIN_WAIT_1:
            if (conn->snd_una == conn->snd_nxt) {
                conn->state = TCP_STATE_TIME_WAIT;
                cprintf("TCP: Received FIN, state=TIME_WAIT\n");
            } else {
                conn->state = TCP_STATE_CLOSING;
                cprintf("TCP: Received FIN, state=CLOSING\n");
            }
            break;
            
        case TCP_STATE_FIN_WAIT_2:
            conn->state = TCP_STATE_TIME_WAIT;
            cprintf("TCP: Received FIN, state=TIME_WAIT\n");
            break;
            
        default:
            cprintf("TCP: FIN received in unexpected state: %d\n", conn->state);
            return -E_UNSUPPORTED;
    }
    return 0;
}
int
handle_tcp(TcpHdr *hdr, size_t length, Ipv4Addr src_ip, Ipv4Addr dst_ip) {
    cprintf("%s: entry\n", __func__);
    if (length < sizeof(TcpHdr)) {
        cprintf("%s: TCP packet too short: %lu bytes\n", __func__, length);
        return -E_INVALID_PACKET;
    }
    
    uint16_t tcp_len = tcp_header_length(hdr);
    if (tcp_len < sizeof(TcpHdr) || tcp_len > length) {
        cprintf("%s: Invalid TCP header length: %u (packet length: %lu)\n", 
                __func__, tcp_len, length);
        return -E_INVALID_PACKET;
    }
    
    uint8_t *payload = (uint8_t*)hdr + tcp_len;
    uint16_t payload_len = length - tcp_len;
    
    uint32_t src_ip_val = src_ip.addr;
    uint32_t dst_ip_val = dst_ip.addr;
    uint16_t src_port = ntoh16(hdr->src_port);
    uint16_t dst_port = ntoh16(hdr->dst_port);
    TcpConnection *conn = tcp_find_connection(src_ip_val, src_port, 
                                              dst_ip_val, dst_port);
    if (!conn && (hdr->flags & TCP_FLAG_SYN) && !(hdr->flags & TCP_FLAG_ACK)) {
        conn = tcp_find_connection(0, 0, dst_ip_val, dst_port);
        
        if (conn && conn->state == TCP_STATE_LISTEN) {
            TcpConnection *new_conn = tcp_create_connection(
                dst_ip_val, dst_port, src_ip_val, src_port);
            
            if (new_conn) {
                new_conn->state = TCP_STATE_SYN_RCVD;
                conn = new_conn;
                cprintf("TCP: Created new connection for client\n");
            } else {
                cprintf("TCP: Failed to create new connection\n");
                return -E_NO_MEM;
            }
        }
    }
    
    if (!conn) {
        cprintf("%s: No connection found for TCP packet\n", __func__);
        return -E_UNSUPPORTED;
    }
    
    uint32_t seq = ntoh32(hdr->seq_num);
    uint32_t ack = ntoh32(hdr->ack_num);
    
    cprintf("TCP packet: flags=0x%02x seq=%u ack=%u len=%u state=%d\n",
            hdr->flags, seq, ack, payload_len, conn->state);
    if (hdr->flags & TCP_FLAG_RST) {
        cprintf("TCP: Connection reset received\n");
        tcp_free_connection(conn);
        return 0;
    }
    if (hdr->flags & TCP_FLAG_SYN) {
        if (hdr->flags & TCP_FLAG_ACK) {
            return handle_tcp_syn_ack(conn, hdr, seq, ack);
        } else {
            return handle_tcp_syn(conn, hdr, seq, src_ip_val);
        }
    }
    if (hdr->flags & TCP_FLAG_ACK) {
        return handle_tcp_ack(conn, hdr, ack, payload, payload_len);
    }
    if (hdr->flags & TCP_FLAG_FIN) {
        return handle_tcp_fin(conn);
    }
    if (payload_len > 0) {
        return handle_tcp_ack(conn, hdr, ack, payload, payload_len);
    }
    
    cprintf("%s: TCP packet with no actionable flags\n", __func__);
    return 0;
}

int
tcp_connect(Ipv4Addr addr, uint16_t port) {
    uint16_t local_port = 1024 + (tcp_connection_used[0] ? 1 : 0);
    
    TcpConnection *conn = tcp_create_connection(
        guest_ipv4_addr.addr, local_port, 
        addr.addr, port);
    
    if (!conn) {
        cprintf("TCP: Failed to create connection for connect\n");
        return -E_NO_MEM;
    }
    
    conn->state = TCP_STATE_SYN_SENT;
    conn->snd_nxt = 3000;
    conn->snd_una = conn->snd_nxt - 1;
    send_tcp_packet(conn, TCP_FLAG_SYN, NULL, 0);
    cprintf("TCP: Initiated connection to %08x:%u\n", addr.addr, port);
    
    return 0;
}

void
umain(int argc, char **argv) {
    for (int i = 0; i < TCP_MAX_CONNECTIONS; i++) {
        tcp_connection_used[i] = false;
    }
    tcp_connections = NULL;
    TcpConnection *http_listen = tcp_create_connection(
        guest_ipv4_addr.addr, 80, 0, 0);
    if (http_listen) {
        http_listen->state = TCP_STATE_LISTEN;
        cprintf("TCP: Listening on port 80\n");
    }

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