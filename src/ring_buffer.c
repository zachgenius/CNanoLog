#include "ring_buffer.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

void rb_init(ring_buffer_t* rb) {
    rb->write_pos = 0;
    rb->read_pos = 0;
    rb->is_full = 0;
}

// Simple binary packet format: [uint32_t length][char* message]
int rb_write(ring_buffer_t* rb, const char* msg, size_t len) {
    if (rb->is_full) {
        return -1; // Buffer is full
    }

    // Check for available space
    size_t available_space;
    if (rb->write_pos >= rb->read_pos) {
        available_space = RING_BUFFER_SIZE - rb->write_pos + rb->read_pos;
    } else {
        available_space = rb->read_pos - rb->write_pos;
    }

    uint32_t packet_len = (uint32_t)len + 1; // +1 for null terminator
    if (available_space < packet_len + sizeof(uint32_t)) {
        rb->is_full = 1;
        return -1; // Not enough space
    }

    // Write packet length
    uint32_t net_len = packet_len; // Assuming same endianness for this MVP
    if (rb->write_pos + sizeof(uint32_t) <= RING_BUFFER_SIZE) {
        memcpy(&rb->data[rb->write_pos], &net_len, sizeof(uint32_t));
    } else { // Wrap around for length
        size_t part1 = RING_BUFFER_SIZE - rb->write_pos;
        memcpy(&rb->data[rb->write_pos], &net_len, part1);
        memcpy(&rb->data[0], ((char*)&net_len) + part1, sizeof(uint32_t) - part1);
    }
    rb->write_pos = (rb->write_pos + sizeof(uint32_t)) % RING_BUFFER_SIZE;

    // Write message
    if (rb->write_pos + packet_len <= RING_BUFFER_SIZE) {
        memcpy(&rb->data[rb->write_pos], msg, packet_len);
    } else { // Wrap around for message
        size_t part1 = RING_BUFFER_SIZE - rb->write_pos;
        memcpy(&rb->data[rb->write_pos], msg, part1);
        memcpy(&rb->data[0], msg + part1, packet_len - part1);
    }
    rb->write_pos = (rb->write_pos + packet_len) % RING_BUFFER_SIZE;

    if (rb->write_pos == rb->read_pos) {
        rb->is_full = 1;
    }

    return 0;
}

size_t rb_read(ring_buffer_t* rb, char* out_buf, size_t max_len) {
    if (!rb->is_full && rb->read_pos == rb->write_pos) {
        return 0; // Buffer is empty
    }

    // Read packet length
    uint32_t packet_len;
    if (rb->read_pos + sizeof(uint32_t) <= RING_BUFFER_SIZE) {
        memcpy(&packet_len, &rb->data[rb->read_pos], sizeof(uint32_t));
    } else { // Wrap around for length
        size_t part1 = RING_BUFFER_SIZE - rb->read_pos;
        memcpy(&packet_len, &rb->data[rb->read_pos], part1);
        memcpy(((char*)&packet_len) + part1, &rb->data[0], sizeof(uint32_t) - part1);
    }
    rb->read_pos = (rb->read_pos + sizeof(uint32_t)) % RING_BUFFER_SIZE;

    if (packet_len > max_len) {
        // Packet too large for output buffer, something is wrong.
        // For this MVP, we'll just discard it to prevent buffer overflows.
        rb->read_pos = (rb->read_pos + packet_len) % RING_BUFFER_SIZE;
        rb->is_full = 0;
        return 0;
    }

    // Read message
    if (rb->read_pos + packet_len <= RING_BUFFER_SIZE) {
        memcpy(out_buf, &rb->data[rb->read_pos], packet_len);
    } else { // Wrap around for message
        size_t part1 = RING_BUFFER_SIZE - rb->read_pos;
        memcpy(out_buf, &rb->data[rb->read_pos], part1);
        memcpy(out_buf + part1, &rb->data[0], packet_len - part1);
    }
    rb->read_pos = (rb->read_pos + packet_len) % RING_BUFFER_SIZE;
    rb->is_full = 0;

    return packet_len;
}
