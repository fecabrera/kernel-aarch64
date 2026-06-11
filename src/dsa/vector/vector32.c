#include "vector32.h"
#include <mm/heap.h>
#include <string.h>

static void _vector32_grow(struct vector32 *vec) {
    size_t new_capacity = vec->capacity * 2;
    uint32_t *new_data = (uint32_t *)kmalloc(new_capacity * sizeof(uint32_t));

    memcpy(new_data, vec->data, vec->length * sizeof(uint32_t));
    kfree(vec->data);

    vec->data = new_data;
    vec->capacity = new_capacity;
}

void vector32_init(struct vector32 *vec, size_t capacity) {
    vec->data = (uint32_t *)kmalloc(capacity * sizeof(uint32_t));
    vec->length = 0;
    vec->capacity = capacity;
}

void vector32_destroy(struct vector32 *vec) {
    kfree(vec->data);

    vec->data = NULL;
    vec->length = 0;
    vec->capacity = 0;
}

void vector32_reset(struct vector32 *vec) {
    vec->length = 0;
}

int vector32_get(struct vector32 *vec, size_t pos, uint32_t *out) {
    if (pos >= vec->length)
        return -1;

    *out = vec->data[pos];
    return 0;
}

int vector32_set(struct vector32 *vec, size_t pos, uint32_t value) {
    if (pos >= vec->length)
        return -1;

    vec->data[pos] = value;
    return 0;
}

void vector32_append(struct vector32 *vec, uint32_t value) {
    if (vec->length == vec->capacity)
        _vector32_grow(vec);

    vec->data[vec->length++] = value;
}
