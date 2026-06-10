#include <mm/heap.h>
#include <string.h>
#include "queue8.h"

void _queue8_grow(struct queue8 *q)
{
    size_t growth = q->capacity * 6 / 10;
    size_t capacity = q->capacity + (growth > 0 ? growth : 1);
    uint8_t *data = (uint8_t *)kmalloc(capacity * sizeof(uint8_t));

    for (size_t i = 0; i < q->length; i++)
        data[i] = q->data[(q->head + i) % q->capacity];

    kfree(q->data);

    q->data = data;
    q->head = 0;
    q->capacity = capacity;
}

void queue8_init(struct queue8 *q, size_t capacity)
{
    q->data = (uint8_t *)kmalloc(capacity * sizeof(uint8_t));
    q->head = 0;
    q->length = 0;
    q->capacity = capacity;
}

void queue8_destroy(struct queue8 *q)
{
    kfree(q->data);

    q->data = NULL;
    q->head = 0;
    q->length = 0;
    q->capacity = 0;
}

uint8_t queue8_at(struct queue8 *q, size_t index)
{
    size_t pos = (q->head + index) % q->capacity;
    return q->data[pos];
}

void queue8_push(struct queue8 *q, uint8_t value)
{
    if (q->length == q->capacity)
        _queue8_grow(q);

    size_t pos = (q->head + q->length) % q->capacity;

    q->length++;
    q->data[pos] = value;
}

uint8_t queue8_pop(struct queue8 *q)
{
    size_t pos = q->head;

    q->head = (q->head + 1) % q->capacity;
    q->length--;

    return q->data[pos];
}

uint8_t queue8_peek(struct queue8 *q)
{
    return q->data[q->head];
}
