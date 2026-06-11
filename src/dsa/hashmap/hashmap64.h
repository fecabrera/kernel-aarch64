#pragma once

#include <stdint.h>
#include <string.h>

#define HASHMAP64_SLOT_EMPTY 0
#define HASHMAP64_SLOT_OCCUPIED 1
#define HASHMAP64_SLOT_TOMBSTONE 2

// Open-addressing hash table with linear probing; maps null-terminated string keys to uint64_t
// values. Keys are duplicated on insert and freed on remove or destroy. Grows automatically when
// the load factor reaches 70%.
struct hashmap64_entry {
    char *key;
    uint64_t value;
    uint8_t state;
};

struct hashmap64 {
    struct hashmap64_entry *entries; // heap-allocated slot array
    size_t length;                   // number of live entries
    size_t capacity;                 // total allocated slots
};

/**
 * Allocates the backing slot array and initialises an empty map.
 *
 * @param map:      map to initialise
 * @param capacity: initial slot count; must be > 0
 */
void hashmap64_init(struct hashmap64 *map, size_t capacity);

/**
 * Frees all duplicated keys and the backing slot array, then zeroes the map fields.
 *
 * @param map: map to destroy
 */
void hashmap64_destroy(struct hashmap64 *map);

/**
 * Inserts or updates the entry for key. The key is duplicated into a heap-allocated buffer. Grows
 * the backing array if the load factor reaches 70%.
 *
 * @param map:   map to insert into
 * @param key:   null-terminated key string (duplicated)
 * @param value: value to associate with key
 */
void hashmap64_set(struct hashmap64 *map, char *key, uint64_t value);

/**
 * Looks up key and writes the associated value into *out if found.
 *
 * @param map: map to search
 * @param key: null-terminated key string
 * @param out: written with the found value; unchanged if key is absent
 *
 * @return 1 if key was found, 0 otherwise
 */
int hashmap64_get(struct hashmap64 *map, char *key, uint64_t *out);

/**
 * Returns 1 if key is present in the map, 0 otherwise.
 *
 * @param map: map to search
 * @param key: null-terminated key string
 */
int hashmap64_has(struct hashmap64 *map, char *key);

/**
 * Removes the entry for key, freeing its duplicated key string. Does nothing if key is not present.
 *
 * @param map: map to remove from
 * @param key: null-terminated key string
 */
void hashmap64_remove(struct hashmap64 *map, char *key);
