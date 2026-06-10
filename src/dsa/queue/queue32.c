#include <mm/heap.h>
#include <string.h>
#include "queue32.h"

void _queue32_grow(struct queue32 *q)
{
    size_t growth = q->capacity * 6 / 10;
    size_t capacity = q->capacity + (growth > 0 ? growth : 1);
    uint32_t *data = (uint32_t *)kmalloc(capacity * sizeof(uint32_t));

    for (size_t i = 0; i < q->length; i++)
        data[i] = q->data[(q->head + i) % q->capacity];

    kfree(q->data);

    q->data = data;
    q->head = 0;
    q->capacity = capacity;
}

void queue32_init(struct queue32 *q, size_t capacity)
{
    q->data = (uint32_t *)kmalloc(capacity * sizeof(uint32_t));
    q->head = 0;
    q->length = 0;
    q->capacity = capacity;
}

void queue32_destroy(struct queue32 *q)
{
    kfree(q->data);

    q->data = NULL;
    q->head = 0;
    q->length = 0;
    q->capacity = 0;
}

uint32_t queue32_at(struct queue32 *q, size_t index)
{
    size_t pos = (q->head + index) % q->capacity;
    return q->data[pos];
}

void queue32_push(struct queue32 *q, uint32_t value)
{
    if (q->length == q->capacity)
        _queue32_grow(q);

    size_t pos = (q->head + q->length) % q->capacity;

    q->length++;
    q->data[pos] = value;
}

uint32_t queue32_pop(struct queue32 *q)
{
    size_t pos = q->head;

    q->head = (q->head + 1) % q->capacity;
    q->length--;

    return q->data[pos];
}

uint32_t queue32_peek(struct queue32 *q)
{
    return q->data[q->head];
}
