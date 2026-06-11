#pragma once // IWYU pragma: private, include "dsa/set.h"

#include <stdint.h>
#include <string.h>

#define SET16_SLOT_EMPTY 0
#define SET16_SLOT_OCCUPIED 1
#define SET16_SLOT_TOMBSTONE 2

// Open-addressing hash table with linear probing; maps uint16_t keys to uint16_t values. Grows
// automatically when the load factor reaches 70%.
struct set16_entry {
    uint16_t key;
    uint16_t value;
    uint8_t state;
};

struct set16 {
    struct set16_entry *entries; // heap-allocated slot array
    size_t length;               // number of live entries
    size_t capacity;             // total allocated slots
};

/**
 * Allocates the backing slot array and initialises an empty set.
 *
 * @param set:      set to initialise
 * @param capacity: initial slot count; must be > 0
 */
void set16_init(struct set16 *set, size_t capacity);

/**
 * Frees the backing slot array and zeroes the set fields.
 *
 * @param set: set to destroy
 */
void set16_destroy(struct set16 *set);

/**
 * Inserts or updates the entry for key. Grows the backing array if the load factor reaches 70%.
 *
 * @param set:   set to insert into
 * @param key:   key to insert or update
 * @param value: value to associate with key
 */
void set16_set(struct set16 *set, uint16_t key, uint16_t value);

/**
 * Looks up key and writes the associated value into *out if found.
 *
 * @param set: set to search
 * @param key: key to look up
 * @param out: written with the found value; unchanged if key is absent
 *
 * @return 1 if key was found, 0 otherwise
 */
int set16_get(struct set16 *set, uint16_t key, uint16_t *out);

/**
 * Removes the entry for key. Does nothing if key is not present.
 *
 * @param set: set to remove from
 * @param key: key to remove
 */
void set16_remove(struct set16 *set, uint16_t key);
