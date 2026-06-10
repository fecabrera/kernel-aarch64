#include <mm/heap.h>
#include <string.h>
#include "queue64.h"

void _queue64_grow(struct queue64 *q)
{
    size_t growth = q->capacity * 6 / 10;
    size_t capacity = q->capacity + (growth > 0 ? growth : 1);
    uint64_t *data = (uint64_t *)kmalloc(capacity * sizeof(uint64_t));

    for (size_t i = 0; i < q->length; i++)
        data[i] = q->data[(q->head + i) % q->capacity];

    kfree(q->data);

    q->data = data;
    q->head = 0;
    q->capacity = capacity;
}

void queue64_init(struct queue64 *q, size_t capacity)
{
    q->data = (uint64_t *)kmalloc(capacity * sizeof(uint64_t));
    q->head = 0;
    q->length = 0;
    q->capacity = capacity;
}

void queue64_destroy(struct queue64 *q)
{
    kfree(q->data);

    q->data = NULL;
    q->head = 0;
    q->length = 0;
    q->capacity = 0;
}

uint64_t queue64_at(struct queue64 *q, size_t index)
{
    size_t pos = (q->head + index) % (q->capacity);
    return q->data[pos];
}

void queue64_push(struct queue64 *q, uint64_t value)
{
    if (q->length == q->capacity)
        _queue64_grow(q);

    size_t pos = (q->head + q->length) % (q->capacity);

    q->length++;
    q->data[pos] = value;
}

uint64_t queue64_pop(struct queue64 *q)
{
    size_t pos = q->head;

    q->head = (q->head + 1) % q->capacity;
    q->length--;

    return q->data[pos];
}

uint64_t queue64_peek(struct queue64 *q)
{
    return q->data[q->head];
}