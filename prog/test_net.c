#include <inc/types.h>

int (*volatile cprintf)(const char *fmt, ...);
int (*volatile net_read)(void*, size_t*);
int (*volatile net_write)(void*, size_t);
void (*volatile packet_dump)(const uint8_t *const, const size_t);

/* AI generated test data */
/* Ethernet (14) + ARP (28) = 42 bytes */
uint8_t eth_arp_pkt[] = {
    /* Ethernet header */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* Destination MAC: broadcast */
    0x52, 0x54, 0x00, 0x12, 0x34, 0x57, /* Source MAC: virtio-net MAC */
    0x08, 0x06,                         /* EtherType: ARP (0x0806) */

    /* ARP header */
    0x00, 0x01,                         /* Hardware type: Ethernet */
    0x08, 0x00,                         /* Protocol type: IPv4 */
    0x06,                               /* Hardware size (MAC) */
    0x04,                               /* Protocol size (IPv4) */
    0x00, 0x01,                         /* Opcode: request */

    /* Sender MAC */
    0x52, 0x54, 0x00, 0x12, 0x34, 0x57,
    /* Sender IP */
    0x0a, 0x00, 0x00, 0x01,             /* 10.0.0.1 */

    /* Target MAC (unknown) */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    /* Target IP */
    0x0a, 0x00, 0x00, 0x02              /* 10.0.0.2 */
};

void
umain(int argc, char **argv) {
    for (;;) {
        {
            cprintf("Writing data to virtio-net...\n");

            int err = net_write(eth_arp_pkt, sizeof(eth_arp_pkt));
            if (err != 0) {
                cprintf("Failed to net write with error = %d\n", err);
                break;
            }

            cprintf("Written to virtio-net:\n");
            packet_dump(eth_arp_pkt, sizeof(eth_arp_pkt));
            cprintf("\n");
        }

        {
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
            cprintf("\n");
        }
    }
}
