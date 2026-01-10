#ifndef JOS_INC_NET_H
#define JOS_INC_NET_H

#include <inc/types.h>

int init_network();

int net_read(void *buf, size_t *size);
int net_write(void *buf, size_t size);

void packet_dump(const uint8_t *const buf, const size_t buf_size);

#endif // JOS_INC_NET_H
