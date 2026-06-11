#pragma once // IWYU pragma: private, include "dsa/vector.h"

#include <stddef.h>
#include <stdint.h>

// Dynamic array of uint8_t values with contiguous storage.
// Grows automatically on append; never shrinks.
struct vector8 {
    uint8_t *data;  // heap-allocated element buffer
    size_t length;  // number of live elements
    size_t capacity; // total allocated slots
};

/**
 * Allocates the backing buffer and initialises an empty vector.
 *
 * @param vec:      vector to initialise
 * @param capacity: initial slot count; must be > 0
 */
void vector8_init(struct vector8 *vec, size_t capacity);

/**
 * Frees the backing buffer and zeroes the vector fields.
 *
 * @param vec: vector to destroy
 */
void vector8_destroy(struct vector8 *vec);

/**
 * Sets length to 0 without freeing the backing buffer. Capacity is unchanged.
 *
 * @param vec: vector to reset
 */
void vector8_reset(struct vector8 *vec);

/**
 * Reads the element at pos into *out.
 *
 * @param vec: vector to read from
 * @param pos: zero-based index; must be < vec->length
 * @param out: written with the element value
 *
 * @return 0 on success, -1 if pos is out of bounds
 */
int vector8_get(struct vector8 *vec, size_t pos, uint8_t *out);

/**
 * Overwrites the element at pos with value.
 *
 * @param vec:   vector to write into
 * @param pos:   zero-based index; must be < vec->length
 * @param value: value to store
 *
 * @return 0 on success, -1 if pos is out of bounds
 */
int vector8_set(struct vector8 *vec, size_t pos, uint8_t value);

/**
 * Appends value to the end of the vector, growing the backing buffer if needed.
 *
 * @param vec:   vector to append to
 * @param value: value to append
 */
void vector8_append(struct vector8 *vec, uint8_t value);
