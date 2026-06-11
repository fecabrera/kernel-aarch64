#include "vector8.h"
#include <mm/heap.h>
#include <string.h>

static void _vector8_grow(struct vector8 *vec) {
    size_t new_capacity = vec->capacity * 2;
    uint8_t *new_data = (uint8_t *)kmalloc(new_capacity * sizeof(uint8_t));

    memcpy(new_data, vec->data, vec->length * sizeof(uint8_t));
    kfree(vec->data);

    vec->data = new_data;
    vec->capacity = new_capacity;
}

void vector8_init(struct vector8 *vec, size_t capacity) {
    vec->data = (uint8_t *)kmalloc(capacity * sizeof(uint8_t));
    vec->length = 0;
    vec->capacity = capacity;
}

void vector8_destroy(struct vector8 *vec) {
    kfree(vec->data);

    vec->data = NULL;
    vec->length = 0;
    vec->capacity = 0;
}

void vector8_reset(struct vector8 *vec) {
    vec->length = 0;
}

int vector8_get(struct vector8 *vec, size_t pos, uint8_t *out) {
    if (pos >= vec->length)
        return -1;

    *out = vec->data[pos];
    return 0;
}

int vector8_set(struct vector8 *vec, size_t pos, uint8_t value) {
    if (pos >= vec->length)
        return -1;

    vec->data[pos] = value;
    return 0;
}

void vector8_append(struct vector8 *vec, uint8_t value) {
    if (vec->length == vec->capacity)
        _vector8_grow(vec);

    vec->data[vec->length++] = value;
}
