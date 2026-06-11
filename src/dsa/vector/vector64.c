#include "vector64.h"
#include <mm/heap.h>
#include <string.h>

static void _vector64_grow(struct vector64 *vec) {
    size_t new_capacity = vec->capacity * 2;
    uint64_t *new_data = (uint64_t *)kmalloc(new_capacity * sizeof(uint64_t));

    memcpy(new_data, vec->data, vec->length * sizeof(uint64_t));
    kfree(vec->data);

    vec->data = new_data;
    vec->capacity = new_capacity;
}

void vector64_init(struct vector64 *vec, size_t capacity) {
    vec->data = (uint64_t *)kmalloc(capacity * sizeof(uint64_t));
    vec->length = 0;
    vec->capacity = capacity;
}

void vector64_destroy(struct vector64 *vec) {
    kfree(vec->data);

    vec->data = NULL;
    vec->length = 0;
    vec->capacity = 0;
}

void vector64_reset(struct vector64 *vec) {
    vec->length = 0;
}

int vector64_get(struct vector64 *vec, size_t pos, uint64_t *out) {
    if (pos >= vec->length)
        return -1;

    *out = vec->data[pos];
    return 0;
}

int vector64_set(struct vector64 *vec, size_t pos, uint64_t value) {
    if (pos >= vec->length)
        return -1;

    vec->data[pos] = value;
    return 0;
}

void vector64_append(struct vector64 *vec, uint64_t value) {
    if (vec->length == vec->capacity)
        _vector64_grow(vec);

    vec->data[vec->length++] = value;
}
