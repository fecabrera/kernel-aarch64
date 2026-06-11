#include "stack16.h"
#include <mm/heap.h>
#include <string.h>

void _stack16_grow(struct stack16 *s) {
    uint16_t growth = s->capacity * 6 / 10;
    uint16_t capacity = s->capacity + (growth > 0 ? growth : 1);
    uint16_t *data = (uint16_t *)kmalloc(s->top * sizeof(uint16_t));

    memcpy(data, s->data, s->top * sizeof(uint16_t));
    kfree(s->data);

    s->data = data;
    s->capacity = capacity;
}

void stack16_init(struct stack16 *s, size_t capacity) {
    s->data = (uint16_t *)kmalloc(capacity * sizeof(uint16_t));
    s->top = 0;
    s->capacity = capacity;
}

void stack16_destroy(struct stack16 *s) {
    kfree(s->data);

    s->data = NULL;
    s->top = 0;
    s->capacity = 0;
}

void stack16_push(struct stack16 *s, uint16_t value) {
    if (s->top == s->capacity)
        _stack16_grow(s);

    s->data[s->top++] = value;
}

uint16_t stack16_pop(struct stack16 *s) { return s->data[--s->top]; }

uint16_t stack16_peek(struct stack16 *s) { return s->data[s->top - 1]; }
