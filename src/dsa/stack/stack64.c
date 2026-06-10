#include <mm/heap.h>
#include <string.h>
#include "stack64.h"

void _stack64_grow(struct stack64 *s)
{
    uint64_t growth = s->capacity * 6 / 10;
    uint64_t capacity = s->capacity + (growth > 0 ? growth : 1);
    uint64_t *data = (uint64_t *)kmalloc(s->top * sizeof(uint64_t));

    memcpy(data, s->data, s->top * sizeof(uint64_t));
    kfree(s->data);

    s->data = data;
    s->capacity = capacity;
}

void stack64_init(struct stack64 *s, size_t capacity)
{
    s->data = (uint64_t *)kmalloc(capacity * sizeof(uint64_t));
    s->top = 0;
    s->capacity = capacity;
}

void stack64_destroy(struct stack64 *s)
{
    kfree(s->data);

    s->data = NULL;
    s->top = 0;
    s->capacity = 0;
}

void stack64_push(struct stack64 *s, uint64_t value)
{
    if (s->top == s->capacity)
        _stack64_grow(s);

    s->data[s->top++] = value;
}

uint64_t stack64_pop(struct stack64 *s)
{
    return s->data[--s->top];
}

uint64_t stack64_peek(struct stack64 *s)
{
    return s->data[s->top - 1];
}