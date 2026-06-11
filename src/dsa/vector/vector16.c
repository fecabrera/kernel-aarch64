#include "vector16.h"
#include <mm/heap.h>
#include <string.h>

static void _vector16_grow(struct vector16 *vec) {
    size_t new_capacity = vec->capacity * 2;
    uint16_t *new_data = (uint16_t *)kmalloc(new_capacity * sizeof(uint16_t));

    memcpy(new_data, vec->data, vec->length * sizeof(uint16_t));
    kfree(vec->data);

    vec->data = new_data;
    vec->capacity = new_capacity;
}

void vector16_init(struct vector16 *vec, size_t capacity) {
    vec->data = (uint16_t *)kmalloc(capacity * sizeof(uint16_t));
    vec->length = 0;
    vec->capacity = capacity;
}

void vector16_destroy(struct vector16 *vec) {
    kfree(vec->data);

    vec->data = NULL;
    vec->length = 0;
    vec->capacity = 0;
}

void vector16_reset(struct vector16 *vec) {
    vec->length = 0;
}

int vector16_get(struct vector16 *vec, size_t pos, uint16_t *out) {
    if (pos >= vec->length)
        return -1;

    *out = vec->data[pos];
    return 0;
}

int vector16_set(struct vector16 *vec, size_t pos, uint16_t value) {
    if (pos >= vec->length)
        return -1;

    vec->data[pos] = value;
    return 0;
}

void vector16_append(struct vector16 *vec, uint16_t value) {
    if (vec->length == vec->capacity)
        _vector16_grow(vec);

    vec->data[vec->length++] = value;
}
