#pragma once

#include <stdint.h>
#include <string.h>

// Dynamic array-backed LIFO stack of uint32_t values.
// Grows automatically when full; never shrinks.
struct stack32 {
    uint32_t *data;  // heap-allocated element buffer
    size_t top;      // index of the next free slot (== number of live elements)
    size_t capacity; // total allocated slots
};

/**
 * Allocates the backing buffer and initialises an empty stack.
 *
 * @param s:        stack to initialise
 * @param capacity: initial slot count
 */
void stack32_init(struct stack32 *s, size_t capacity);

/**
 * Frees the backing buffer and zeroes the stack fields.
 *
 * @param s: stack to destroy
 */
void stack32_destroy(struct stack32 *s);

/**
 * Pushes value onto the top of the stack. Grows the backing buffer if full.
 *
 * @param s:     stack to push onto
 * @param value: value to push
 */
void stack32_push(struct stack32 *s, uint32_t value);

/**
 * Removes and returns the top element. Caller must ensure s->top > 0 before calling — behaviour is
 * undefined on an empty stack.
 *
 * @param s: stack to pop from
 *
 * @return the popped value
 */
uint32_t stack32_pop(struct stack32 *s);

/**
 * Returns the top element without removing it. Caller must ensure s->top > 0 before calling —
 * behaviour is undefined on an empty stack.
 *
 * @param s: stack to peek at
 *
 * @return the top value
 */
uint32_t stack32_peek(struct stack32 *s);
