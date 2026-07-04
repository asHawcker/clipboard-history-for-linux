#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdlib.h>
#include <stdint.h>

#define MAX_CLIPS 50
#define MAX_PAYLOAD_SIZE (10 * 1024 * 1024) // Put a 10MB cap on the size of payload

typedef struct
{
    uint32_t id;
    char *data;
    size_t size;
} clip_entry_t;

typedef struct
{
    clip_entry_t entries[MAX_CLIPS];
    int head;
    int tail;
    int count;        // Current number of stored items
    uint32_t next_id; // next ID counter increases monotonically
} ring_buffer_t;

void rb_init(ring_buffer_t *rb);

uint32_t rb_push(ring_buffer_t *rb, const char *data, size_t size);

const clip_entry_t *rb_get(const ring_buffer_t *rb, uint32_t id);

void rb_free(ring_buffer_t *rb);

#endif