#pragma once

#include <stdint.h>
#include <string.h>

// Dynamic ring-buffer FIFO queue of uint32_t values.
// Grows automatically when full; never shrinks.
struct queue32
{
    uint32_t *data;  // heap-allocated element buffer
    size_t head;     // index of the front element
    size_t length;   // number of live elements
    size_t capacity; // total allocated slots
};

/**
 * Allocates the backing buffer and initialises an empty queue.
 *
 * @param q:        queue to initialise
 * @param capacity: initial slot count
 */
void queue32_init(struct queue32 *q, size_t capacity);

/**
 * Frees the backing buffer and zeroes the queue fields.
 *
 * @param q: queue to destroy
 */
void queue32_destroy(struct queue32 *q);

/**
 * Appends value to the back of the queue. Grows the backing buffer if full.
 *
 * @param q:     queue to push onto
 * @param value: value to enqueue
 */
void queue32_push(struct queue32 *q, uint32_t value);

/**
 * Returns the element at logical index without removing it. Index 0 is the
 * front. Caller must ensure index < q->length — behaviour is undefined
 * otherwise.
 *
 * @param q:     queue to index into
 * @param index: logical position from the front
 *
 * @return the value at that position
 */
uint32_t queue32_at(struct queue32 *q, size_t index);

/**
 * Removes and returns the front element. Caller must ensure q->length > 0
 * before calling — behaviour is undefined on an empty queue.
 *
 * @param q: queue to pop from
 *
 * @return the dequeued value
 */
uint32_t queue32_pop(struct queue32 *q);

/**
 * Returns the front element without removing it. Caller must ensure q->length > 0
 * before calling — behaviour is undefined on an empty queue.
 *
 * @param q: queue to peek at
 *
 * @return the front value
 */
uint32_t queue32_peek(struct queue32 *q);
