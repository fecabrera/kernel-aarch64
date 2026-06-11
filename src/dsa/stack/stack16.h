#pragma once

#include <stdint.h>
#include <string.h>

// Dynamic array-backed LIFO stack of uint16_t values.
// Grows automatically when full; never shrinks.
struct stack16 {
    uint16_t *data;  // heap-allocated element buffer
    size_t top;      // index of the next free slot (== number of live elements)
    size_t capacity; // total allocated slots
};

/**
 * Allocates the backing buffer and initialises an empty stack.
 *
 * @param s:        stack to initialise
 * @param capacity: initial slot count
 */
void stack16_init(struct stack16 *s, size_t capacity);

/**
 * Frees the backing buffer and zeroes the stack fields.
 *
 * @param s: stack to destroy
 */
void stack16_destroy(struct stack16 *s);

/**
 * Pushes value onto the top of the stack. Grows the backing buffer if full.
 *
 * @param s:     stack to push onto
 * @param value: value to push
 */
void stack16_push(struct stack16 *s, uint16_t value);

/**
 * Removes and returns the top element. Caller must ensure s->top > 0 before calling — behaviour is
 * undefined on an empty stack.
 *
 * @param s: stack to pop from
 *
 * @return the popped value
 */
uint16_t stack16_pop(struct stack16 *s);

/**
 * Returns the top element without removing it. Caller must ensure s->top > 0 before calling —
 * behaviour is undefined on an empty stack.
 *
 * @param s: stack to peek at
 *
 * @return the top value
 */
uint16_t stack16_peek(struct stack16 *s);
