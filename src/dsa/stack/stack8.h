#pragma once // IWYU pragma: private, include "dsa/stack.h"

#include <stdint.h>
#include <string.h>

// Dynamic array-backed LIFO stack of uint8_t values.
// Grows automatically when full; never shrinks.
struct stack8 {
    uint8_t *data;   // heap-allocated element buffer
    size_t top;      // index of the next free slot (== number of live elements)
    size_t capacity; // total allocated slots
};

/**
 * Allocates the backing buffer and initialises an empty stack.
 *
 * @param s:        stack to initialise
 * @param capacity: initial slot count
 */
void stack8_init(struct stack8 *s, size_t capacity);

/**
 * Frees the backing buffer and zeroes the stack fields.
 *
 * @param s: stack to destroy
 */
void stack8_destroy(struct stack8 *s);

/**
 * Pushes value onto the top of the stack. Grows the backing buffer if full.
 *
 * @param s:     stack to push onto
 * @param value: value to push
 */
void stack8_push(struct stack8 *s, uint8_t value);

/**
 * Removes and returns the top element. Caller must ensure s->top > 0 before calling — behaviour is
 * undefined on an empty stack.
 *
 * @param s: stack to pop from
 *
 * @return the popped value
 */
uint8_t stack8_pop(struct stack8 *s);

/**
 * Returns the top element without removing it. Caller must ensure s->top > 0 before calling —
 * behaviour is undefined on an empty stack.
 *
 * @param s: stack to peek at
 *
 * @return the top value
 */
uint8_t stack8_peek(struct stack8 *s);
