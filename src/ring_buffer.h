#pragma once

#include <stddef.h>

#define RING_BUFFER_SIZE (1024 * 1024) // 1MB buffer

// A single entry in the buffer. The first 4 bytes store the length of the message.
// This makes reading simpler.
typedef struct {
    char data[RING_BUFFER_SIZE];
    size_t write_pos;
    size_t read_pos;
    int is_full;
} ring_buffer_t;

void rb_init(ring_buffer_t* rb);

// Returns 0 on success, -1 if buffer is full
int rb_write(ring_buffer_t* rb, const char* msg, size_t len);

// Returns number of bytes read, 0 if buffer is empty
size_t rb_read(ring_buffer_t* rb, char* out_buf, size_t max_len);

