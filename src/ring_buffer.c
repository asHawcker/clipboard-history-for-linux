#include "ring_buffer.h"
#include <string.h>

void rb_init(ring_buffer_t *rb)
{
    memset(rb, 0, sizeof(ring_buffer_t));
    rb->next_id = 1;
}

uint32_t rb_push(ring_buffer_t *rb, const char *data, size_t size)
{
    if (!data || size == 0 || size > MAX_PAYLOAD_SIZE)
    {
        return 0;
    }

    // if buffer is full, remove the last in element
    if (rb->count == MAX_CLIPS)
    {
        free(rb->entries[rb->tail].data);
        rb->entries[rb->tail].data = NULL;
        rb->tail = (rb->tail + 1) % MAX_CLIPS;
        rb->count--;
    }

    // allocate new memory
    char *new_data = malloc(size);
    if (!new_data)
    {
        return 0;
    }
    memcpy(new_data, data, size);

    // insert new data at the head
    uint32_t assigned_id = rb->next_id++; // get the new id and increment
    rb->entries[rb->head].id = assigned_id;
    rb->entries[rb->head].size = size;
    rb->entries[rb->head].data = new_data;

    // adjust the head considering the wraparound
    rb->head = (rb->head + 1) % MAX_CLIPS;
    rb->count++;

    return assigned_id;
}

const clip_entry_t *rb_get(const ring_buffer_t *rb, uint32_t id)
{
    for (int i = 0; i < rb->count; i++)
    {
        int idx = (rb->head - i - 1 + MAX_CLIPS) % MAX_CLIPS;
        if (rb->entries[idx].id == id)
        {
            return &rb->entries[idx];
        }
    }
    return NULL;
}

void rb_free(ring_buffer_t *rb)
{
    for (int i = 0; i < rb->count; i++)
    {
        int idx = (rb->tail + i) % MAX_CLIPS;
        free(rb->entries[idx].data);
        rb->entries[idx].data = NULL;
    }

    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}