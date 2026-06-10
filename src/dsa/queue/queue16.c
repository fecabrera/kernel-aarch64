#include <mm/heap.h>
#include <string.h>
#include "queue16.h"

void _queue16_grow(struct queue16 *q)
{
    size_t growth = q->capacity * 6 / 10;
    size_t capacity = q->capacity + (growth > 0 ? growth : 1);
    uint16_t *data = (uint16_t *)kmalloc(capacity * sizeof(uint16_t));

    for (size_t i = 0; i < q->length; i++)
        data[i] = q->data[(q->head + i) % q->capacity];

    kfree(q->data);

    q->data = data;
    q->head = 0;
    q->capacity = capacity;
}

void queue16_init(struct queue16 *q, size_t capacity)
{
    q->data = (uint16_t *)kmalloc(capacity * sizeof(uint16_t));
    q->head = 0;
    q->length = 0;
    q->capacity = capacity;
}

void queue16_destroy(struct queue16 *q)
{
    kfree(q->data);

    q->data = NULL;
    q->head = 0;
    q->length = 0;
    q->capacity = 0;
}

uint16_t queue16_at(struct queue16 *q, size_t index)
{
    size_t pos = (q->head + index) % q->capacity;
    return q->data[pos];
}

void queue16_push(struct queue16 *q, uint16_t value)
{
    if (q->length == q->capacity)
        _queue16_grow(q);

    size_t pos = (q->head + q->length) % q->capacity;

    q->length++;
    q->data[pos] = value;
}

uint16_t queue16_pop(struct queue16 *q)
{
    size_t pos = q->head;

    q->head = (q->head + 1) % q->capacity;
    q->length--;

    return q->data[pos];
}

uint16_t queue16_peek(struct queue16 *q)
{
    return q->data[q->head];
}
