#pragma once

#include <types.h>
#include <string.h>

// Dynamic ring-buffer FIFO queue of uint64_t values.
// Grows automatically when full; never shrinks.
struct queue64
{
    uint64_t *data;  // heap-allocated element buffer
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
void queue64_init(struct queue64 *q, size_t capacity);

/**
 * Frees the backing buffer and zeroes the queue fields.
 *
 * @param q: queue to destroy
 */
void queue64_destroy(struct queue64 *q);

/**
 * Appends value to the back of the queue. Grows the backing buffer if full.
 *
 * @param q:     queue to push onto
 * @param value: value to enqueue
 */
void queue64_push(struct queue64 *q, uint64_t value);

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
uint64_t queue64_at(struct queue64 *q, size_t index);

/**
 * Removes and returns the front element. Caller must ensure q->length > 0
 * before calling — behaviour is undefined on an empty queue.
 *
 * @param q: queue to pop from
 *
 * @return the dequeued value
 */
uint64_t queue64_pop(struct queue64 *q);

/**
 * Returns the front element without removing it. Caller must ensure q->length > 0
 * before calling — behaviour is undefined on an empty queue.
 *
 * @param q: queue to peek at
 *
 * @return the front value
 */
uint64_t queue64_peek(struct queue64 *q);
