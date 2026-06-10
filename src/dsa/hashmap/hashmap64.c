#include <mm/heap.h>
#include <string.h>
#include "hashmap64.h"

static uint64_t _hashmap64_hash(char *key)
{
    uint64_t hash = 14695981039346656037ULL;
    while (*key)
    {
        hash ^= (uint8_t)*key++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static void _hashmap64_grow(struct hashmap64 *map)
{
    size_t old_capacity = map->capacity;
    struct hashmap64_entry *old_entries = map->entries;

    size_t new_capacity = old_capacity * 2;
    struct hashmap64_entry *new_entries = (struct hashmap64_entry *)kmalloc(new_capacity * sizeof(struct hashmap64_entry));

    for (size_t i = 0; i < new_capacity; i++)
        new_entries[i].state = HASHMAP64_SLOT_EMPTY;

    for (size_t i = 0; i < old_capacity; i++)
    {
        if (old_entries[i].state != HASHMAP64_SLOT_OCCUPIED)
            continue;

        size_t slot = _hashmap64_hash(old_entries[i].key) % new_capacity;
        while (new_entries[slot].state == HASHMAP64_SLOT_OCCUPIED)
            slot = (slot + 1) % new_capacity;

        new_entries[slot] = old_entries[i];
    }

    kfree(old_entries);
    map->entries = new_entries;
    map->capacity = new_capacity;
}

void hashmap64_init(struct hashmap64 *map, size_t capacity)
{
    map->entries = (struct hashmap64_entry *)kmalloc(capacity * sizeof(struct hashmap64_entry));
    map->length = 0;
    map->capacity = capacity;

    for (size_t i = 0; i < capacity; i++)
        map->entries[i].state = HASHMAP64_SLOT_EMPTY;
}

void hashmap64_destroy(struct hashmap64 *map)
{
    for (size_t i = 0; i < map->capacity; i++)
    {
        if (map->entries[i].state == HASHMAP64_SLOT_OCCUPIED)
            kfree(map->entries[i].key);
    }

    kfree(map->entries);

    map->entries = NULL;
    map->length = 0;
    map->capacity = 0;
}

void hashmap64_set(struct hashmap64 *map, char *key, uint64_t value)
{
    if (map->length * 10 >= map->capacity * 7)
        _hashmap64_grow(map);

    size_t slot = _hashmap64_hash(key) % map->capacity;
    int has_tombstone = 0;
    size_t tombstone_slot = 0;

    while (map->entries[slot].state != HASHMAP64_SLOT_EMPTY)
    {
        if (map->entries[slot].state == HASHMAP64_SLOT_OCCUPIED &&
            strcmp(map->entries[slot].key, key) == 0)
        {
            map->entries[slot].value = value;
            return;
        }
        if (map->entries[slot].state == HASHMAP64_SLOT_TOMBSTONE && !has_tombstone)
        {
            has_tombstone = 1;
            tombstone_slot = slot;
        }
        slot = (slot + 1) % map->capacity;
    }

    if (has_tombstone)
        slot = tombstone_slot;

    size_t key_len = strlen(key);
    char *key_copy = (char *)kmalloc(key_len + 1);
    strncpy(key_copy, key, key_len);
    key_copy[key_len] = '\0';

    map->entries[slot].key = key_copy;
    map->entries[slot].value = value;
    map->entries[slot].state = HASHMAP64_SLOT_OCCUPIED;
    map->length++;
}

int hashmap64_get(struct hashmap64 *map, char *key, uint64_t *out)
{
    size_t slot = _hashmap64_hash(key) % map->capacity;

    while (map->entries[slot].state != HASHMAP64_SLOT_EMPTY)
    {
        if (map->entries[slot].state == HASHMAP64_SLOT_OCCUPIED &&
            strcmp(map->entries[slot].key, key) == 0)
        {
            *out = map->entries[slot].value;
            return 1;
        }
        slot = (slot + 1) % map->capacity;
    }

    return 0;
}

int hashmap64_has(struct hashmap64 *map, char *key)
{
    uint64_t ignored;
    return hashmap64_get(map, key, &ignored);
}

void hashmap64_remove(struct hashmap64 *map, char *key)
{
    size_t slot = _hashmap64_hash(key) % map->capacity;

    while (map->entries[slot].state != HASHMAP64_SLOT_EMPTY)
    {
        if (map->entries[slot].state == HASHMAP64_SLOT_OCCUPIED &&
            strcmp(map->entries[slot].key, key) == 0)
        {
            kfree(map->entries[slot].key);
            map->entries[slot].key = NULL;
            map->entries[slot].state = HASHMAP64_SLOT_TOMBSTONE;
            map->length--;
            return;
        }
        slot = (slot + 1) % map->capacity;
    }
}
