#pragma once

#include <stdint.h>

// Doubly-linked list deque of uint64_t values.
// Each element is heap-allocated; head points left, tail points right.
struct deque64_entry
{
    uint64_t value;
    struct deque64_entry *prev;
    struct deque64_entry *next;
};

struct deque64
{
    struct deque64_entry *head; // leftmost element, or NULL if empty
    struct deque64_entry *tail; // rightmost element, or NULL if empty
};

/**
 * Inserts value at the left (head) end of the deque.
 *
 * @param dq:    deque to insert into
 * @param value: value to insert
 */
void deque64_add_left(struct deque64 *dq, uint64_t value);

/**
 * Inserts value at the right (tail) end of the deque.
 *
 * @param dq:    deque to insert into
 * @param value: value to insert
 */
void deque64_add_right(struct deque64 *dq, uint64_t value);

/**
 * Removes and returns the leftmost value. Caller must ensure !deque64_is_empty(dq)
 * before calling — behaviour is undefined on an empty deque.
 *
 * @param dq: deque to remove from
 *
 * @return the removed value
 */
uint64_t deque64_remove_left(struct deque64 *dq);

/**
 * Removes and returns the rightmost value. Caller must ensure !deque64_is_empty(dq)
 * before calling — behaviour is undefined on an empty deque.
 *
 * @param dq: deque to remove from
 *
 * @return the removed value
 */
uint64_t deque64_remove_right(struct deque64 *dq);

/**
 * Returns the leftmost value without removing it. Caller must ensure !deque64_is_empty(dq)
 * before calling — behaviour is undefined on an empty deque.
 *
 * @param dq: deque to peek at
 *
 * @return the leftmost value
 */
uint64_t deque64_peek_left(struct deque64 *dq);

/**
 * Returns the rightmost value without removing it. Caller must ensure !deque64_is_empty(dq)
 * before calling — behaviour is undefined on an empty deque.
 *
 * @param dq: deque to peek at
 *
 * @return the rightmost value
 */
uint64_t deque64_peek_right(struct deque64 *dq);

/**
 * Returns non-zero if the deque contains no elements.
 *
 * @param dq: deque to check
 *
 * @return non-zero if empty, 0 otherwise
 */
int deque64_is_empty(struct deque64 *dq);

/**
 * Returns the first entry for which cmp returns 0, or NULL if none match.
 * Search begins at start; if start is NULL, begins at the head.
 *
 * @param dq:    deque to search
 * @param start: entry to start from, or NULL to start from the head
 * @param cmp:   comparator called on each entry; return 0 on match
 * @param ctx:   caller context forwarded to each cmp call
 *
 * @return pointer to the matching entry, or NULL
 */
struct deque64_entry *deque64_find(struct deque64 *dq, struct deque64_entry *start, int (*cmp)(struct deque64_entry *, void *), void *ctx);

/**
 * Finds and unlinks the first entry for which cmp returns 0, or returns NULL
 * if none match. The caller is responsible for freeing the returned entry.
 * Search begins at start; if start is NULL, begins at the head.
 *
 * @param dq:    deque to search and remove from
 * @param start: entry to start from, or NULL to start from the head
 * @param cmp:   comparator called on each entry; return 0 on match
 * @param ctx:   caller context forwarded to each cmp call
 *
 * @return pointer to the removed entry, or NULL
 */
struct deque64_entry *deque64_find_remove(struct deque64 *dq, struct deque64_entry *start, int (*cmp)(struct deque64_entry *, void *), void *ctx);