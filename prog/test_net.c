#include <inc/types.h>

int (*volatile cprintf)(const char *fmt, ...);
int (*volatile net_read)(void*, size_t*);
void (*volatile packet_dump)(const uint8_t *const, const size_t);

void
umain(int argc, char **argv) {
    uint8_t net_buf[128];
    for (;;) {
        size_t net_buf_size = sizeof(net_buf);
        int err = net_read(net_buf, &net_buf_size);
        if (err != 0) {
            cprintf("Failed to read from virtio-net with err = %d\n", err);
            break;
        }

        packet_dump(net_buf, net_buf_size);
        cprintf("\n\n\n");
    }
}
