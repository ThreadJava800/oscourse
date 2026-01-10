#ifndef JOS_INC_RINGBUF_H
#define JOS_INC_RINGBUF_H

#include <inc/mmu.h>
#include <inc/types.h>

typedef struct {
    uint8_t *buffer;
    size_t capacity;

    size_t head;
    size_t tail;

    bool allow_overflow;
} Ringbuf;

int rb_init(Ringbuf *const rb, uint8_t *const storage, const size_t capacity, const bool allow_overflow);
int rb_write(Ringbuf *const rb, const uint8_t *const src, const size_t src_size);
int rb_read(Ringbuf *const rb, uint8_t *const dst, size_t *size);
int rb_is_empty(Ringbuf *const rb);

#endif // JOS_INC_RINGBUF_H
