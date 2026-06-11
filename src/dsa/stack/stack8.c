#include "stack8.h"
#include <mm/heap.h>
#include <string.h>

void _stack8_grow(struct stack8 *s) {
    uint8_t growth = s->capacity * 6 / 10;
    uint8_t capacity = s->capacity + (growth > 0 ? growth : 1);
    uint8_t *data = (uint8_t *)kmalloc(s->top * sizeof(uint8_t));

    memcpy(data, s->data, s->top * sizeof(uint8_t));
    kfree(s->data);

    s->data = data;
    s->capacity = capacity;
}

void stack8_init(struct stack8 *s, size_t capacity) {
    s->data = (uint8_t *)kmalloc(capacity * sizeof(uint8_t));
    s->top = 0;
    s->capacity = capacity;
}

void stack8_destroy(struct stack8 *s) {
    kfree(s->data);

    s->data = NULL;
    s->top = 0;
    s->capacity = 0;
}

void stack8_push(struct stack8 *s, uint8_t value) {
    if (s->top == s->capacity)
        _stack8_grow(s);

    s->data[s->top++] = value;
}

uint8_t stack8_pop(struct stack8 *s) { return s->data[--s->top]; }

uint8_t stack8_peek(struct stack8 *s) { return s->data[s->top - 1]; }
