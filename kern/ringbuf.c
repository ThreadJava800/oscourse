#include <inc/assert.h>
#include <inc/error.h>
#include <inc/ringbuf.h>
#include <inc/string.h>

int
rb_init(Ringbuf *const rb, uint8_t *const storage, const size_t capacity, const bool allow_overflow) {
    if (!rb) {
        cprintf("ringbuf: invalid ringbuf ptr was provided!\n");
        return -E_INVAL;
    }
    if (!storage) {
        cprintf("ringbuf: invalid storage ptr was provided!\n");
        return -E_INVAL;
    }

    rb->buffer = storage;
    rb->capacity = capacity;

    rb->head = 0;
    rb->tail = 0;

    rb->allow_overflow = allow_overflow;
    return 0;
}

int
rb_write(Ringbuf *const rb, const uint8_t *const src, const size_t src_size) {
    if (!rb) {
        cprintf("ringbuf: invalid ringbuf ptr was provided!\n");
        return -E_INVAL;
    }
    if (!src) {
        cprintf("ringbuf: invalid src ptr was provided!\n");
        return -E_INVAL;
    }

    const size_t busy_len = rb->head - rb->tail;
    if (busy_len > rb->capacity) {
        cprintf("ringbuf: busy len exeeds capacity!\n");
        return -E_FAULT;
    }

    if (!rb->allow_overflow) {
        const size_t free_size = rb->capacity - busy_len;
        if (src_size > free_size) {
            cprintf("ringbuf: not enough space to fit data in ringbuf!\n");
            return -E_NO_MEM;
        }
    }

    const size_t chunk = rb->capacity - (rb->head % rb->capacity);
    if (chunk >= src_size) {
        memcpy(rb->buffer + (rb->head % rb->capacity), src, src_size);
    } else {
        memcpy(rb->buffer + (rb->head % rb->capacity), src, chunk);
        memcpy(rb->buffer, src + chunk, src_size - chunk);
    }

    rb->head += src_size;

    if (rb->allow_overflow) {
        if (rb->head - rb->tail > rb->capacity) {
            rb->tail = rb->head - rb->capacity;
        }
    }
    return 0;
}

int
rb_read(Ringbuf *const rb, uint8_t *const dst, size_t *size) {
    if (!rb) {
        cprintf("ringbuf: invalid ringbuf ptr was provided!\n");
        return -E_INVAL;
    }
    if (!dst) {
        cprintf("ringbuf: invalid dst ptr was provided!\n");
        return -E_INVAL;
    }
    if (!size) {
        cprintf("ringbuf: invalid size ptr was provided!\n");
        return -E_INVAL;
    }

    const size_t busy_len = rb->head - rb->tail;
    if (busy_len > rb->capacity) {
        cprintf("ringbuf: busy len exeeds capacity!\n");
        return -E_FAULT;
    }

    const size_t copy_size = MIN(*size, busy_len);

    const size_t chunk = rb->capacity - (rb->tail % rb->capacity);
    if (chunk >= copy_size) {
        memcpy(dst, rb->buffer + (rb->tail % rb->capacity), copy_size);
    } else {
        memcpy(dst, rb->buffer + (rb->tail % rb->capacity), chunk);
        memcpy(dst + chunk, rb->buffer, copy_size - chunk);
    }

    rb->tail += copy_size;

    *size = copy_size;
    return 0;
}

int rb_is_empty(Ringbuf *const rb) {
    if (!rb) {
        cprintf("ringbuf: invalid ringbuf ptr was provided!\n");
        return 1; // report as empty on error
    }

    return rb->head == rb->tail;
}
