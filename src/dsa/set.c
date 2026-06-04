#include <mm/heap.h>
#include "set.h"

// splitmix64 finalizer — good avalanche for integer keys
static uint64_t _set64_hash(uint64_t key)
{
    key ^= key >> 30;
    key *= 0xbf58476d1ce4e5b9ULL;
    key ^= key >> 27;
    key *= 0x94d049bb133111ebULL;
    key ^= key >> 31;
    return key;
}

static void _set64_grow(struct set64 *set)
{
    size_t old_capacity = set->capacity;
    struct set64_entry *old_entries = set->entries;

    size_t new_capacity = old_capacity * 2;
    struct set64_entry *new_entries = (struct set64_entry *)kmalloc(new_capacity * sizeof(struct set64_entry));

    for (size_t i = 0; i < new_capacity; i++)
        new_entries[i].state = SET64_SLOT_EMPTY;

    for (size_t i = 0; i < old_capacity; i++)
    {
        if (old_entries[i].state != SET64_SLOT_OCCUPIED)
            continue;

        size_t slot = _set64_hash(old_entries[i].key) % new_capacity;
        while (new_entries[slot].state == SET64_SLOT_OCCUPIED)
            slot = (slot + 1) % new_capacity;

        new_entries[slot] = old_entries[i];
    }

    kfree(old_entries);
    set->entries = new_entries;
    set->capacity = new_capacity;
}

void set64_init(struct set64 *set, size_t capacity)
{
    set->entries = (struct set64_entry *)kmalloc(capacity * sizeof(struct set64_entry));
    set->length = 0;
    set->capacity = capacity;

    for (size_t i = 0; i < capacity; i++)
        set->entries[i].state = SET64_SLOT_EMPTY;
}

void set64_destroy(struct set64 *set)
{
    kfree(set->entries);

    set->entries = NULL;
    set->length = 0;
    set->capacity = 0;
}

void set64_set(struct set64 *set, uint64_t key, uint64_t value)
{
    if (set->length * 10 >= set->capacity * 7)
        _set64_grow(set);

    size_t slot = _set64_hash(key) % set->capacity;
    int has_tombstone = 0;
    size_t tombstone_slot = 0;

    while (set->entries[slot].state != SET64_SLOT_EMPTY)
    {
        if (set->entries[slot].state == SET64_SLOT_OCCUPIED &&
            set->entries[slot].key == key)
        {
            set->entries[slot].value = value;
            return;
        }
        if (set->entries[slot].state == SET64_SLOT_TOMBSTONE && !has_tombstone)
        {
            has_tombstone = 1;
            tombstone_slot = slot;
        }
        slot = (slot + 1) % set->capacity;
    }

    if (has_tombstone)
        slot = tombstone_slot;

    set->entries[slot].key = key;
    set->entries[slot].value = value;
    set->entries[slot].state = SET64_SLOT_OCCUPIED;
    set->length++;
}

int set64_get(struct set64 *set, uint64_t key, uint64_t *out)
{
    size_t slot = _set64_hash(key) % set->capacity;

    while (set->entries[slot].state != SET64_SLOT_EMPTY)
    {
        if (set->entries[slot].state == SET64_SLOT_OCCUPIED &&
            set->entries[slot].key == key)
        {
            *out = set->entries[slot].value;
            return 1;
        }
        slot = (slot + 1) % set->capacity;
    }

    return 0;
}

void set64_remove(struct set64 *set, uint64_t key)
{
    size_t slot = _set64_hash(key) % set->capacity;

    while (set->entries[slot].state != SET64_SLOT_EMPTY)
    {
        if (set->entries[slot].state == SET64_SLOT_OCCUPIED &&
            set->entries[slot].key == key)
        {
            set->entries[slot].state = SET64_SLOT_TOMBSTONE;
            set->length--;
            return;
        }
        slot = (slot + 1) % set->capacity;
    }
}
