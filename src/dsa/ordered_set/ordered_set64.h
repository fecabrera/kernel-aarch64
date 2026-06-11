#pragma once // IWYU pragma: private, include "dsa/ordered_set.h"

#include <stdint.h>
#include <string.h>

// BST-based ordered set of unique uint64_t values.
// Iteration and min/max operate in sorted order; no balancing is performed.
struct ordered_set64_node {
    uint64_t value;
    struct ordered_set64_node *left;
    struct ordered_set64_node *right;
};

struct ordered_set64 {
    struct ordered_set64_node *root;
    size_t length; // number of live elements
};

/**
 * Initialises an empty set.
 *
 * @param set: set to initialise
 */
void ordered_set64_init(struct ordered_set64 *set);

/**
 * Frees all nodes and zeroes the set fields.
 *
 * @param set: set to destroy
 */
void ordered_set64_destroy(struct ordered_set64 *set);

/**
 * Inserts value into the set. Does nothing if value is already present.
 *
 * @param set:   set to insert into
 * @param value: value to insert
 *
 * @return 1 if value was inserted, 0 if it was already present
 */
int ordered_set64_insert(struct ordered_set64 *set, uint64_t value);

/**
 * Removes value from the set. Does nothing if value is not present.
 *
 * @param set:   set to remove from
 * @param value: value to remove
 */
void ordered_set64_remove(struct ordered_set64 *set, uint64_t value);

/**
 * Returns non-zero if value is present in the set.
 *
 * @param set:   set to search
 * @param value: value to look up
 *
 * @return 1 if found, 0 otherwise
 */
int ordered_set64_contains(struct ordered_set64 *set, uint64_t value);

/**
 * Returns the smallest value in the set. Caller must ensure set->length > 0
 * before calling — behaviour is undefined on an empty set.
 *
 * @param set: set to query
 *
 * @return the minimum value
 */
uint64_t ordered_set64_min(struct ordered_set64 *set);

/**
 * Returns the largest value in the set. Caller must ensure set->length > 0 before calling —
 * behaviour is undefined on an empty set.
 *
 * @param set: set to query
 *
 * @return the maximum value
 */
uint64_t ordered_set64_max(struct ordered_set64 *set);

/**
 * Calls fn(value, ctx) for every element in ascending order.
 *
 * @param set: set to iterate
 * @param fn:  callback invoked once per element
 * @param ctx: opaque pointer forwarded to every fn call
 */
void ordered_set64_foreach(struct ordered_set64 *set, void (*fn)(uint64_t, void *), void *ctx);
