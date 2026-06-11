#include "stack32.h"
#include <mm/heap.h>
#include <string.h>

void _stack32_grow(struct stack32 *s) {
    uint32_t growth = s->capacity * 6 / 10;
    uint32_t capacity = s->capacity + (growth > 0 ? growth : 1);
    uint32_t *data = (uint32_t *)kmalloc(s->top * sizeof(uint32_t));

    memcpy(data, s->data, s->top * sizeof(uint32_t));
    kfree(s->data);

    s->data = data;
    s->capacity = capacity;
}

void stack32_init(struct stack32 *s, size_t capacity) {
    s->data = (uint32_t *)kmalloc(capacity * sizeof(uint32_t));
    s->top = 0;
    s->capacity = capacity;
}

void stack32_destroy(struct stack32 *s) {
    kfree(s->data);

    s->data = NULL;
    s->top = 0;
    s->capacity = 0;
}

void stack32_push(struct stack32 *s, uint32_t value) {
    if (s->top == s->capacity)
        _stack32_grow(s);

    s->data[s->top++] = value;
}

uint32_t stack32_pop(struct stack32 *s) { return s->data[--s->top]; }

uint32_t stack32_peek(struct stack32 *s) { return s->data[s->top - 1]; }
