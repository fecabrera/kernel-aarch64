#include "set32.h"
#include "stdbool.h"
#include <mm/heap.h>

// murmur3 32-bit finalizer
static uint32_t _set32_hash(uint32_t key) {
    key ^= key >> 16;
    key *= 0x45d9f3bU;
    key ^= key >> 16;
    return key;
}

static void _set32_grow(struct set32 *set) {
    size_t old_capacity = set->capacity;
    struct set32_entry *old_entries = set->entries;

    size_t new_capacity = old_capacity * 2;
    struct set32_entry *new_entries =
        (struct set32_entry *)kmalloc(new_capacity * sizeof(struct set32_entry));

    for (size_t i = 0; i < new_capacity; i++)
        new_entries[i].state = SET32_SLOT_EMPTY;

    for (size_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].state != SET32_SLOT_OCCUPIED)
            continue;

        size_t slot = _set32_hash(old_entries[i].key) % new_capacity;
        while (new_entries[slot].state == SET32_SLOT_OCCUPIED)
            slot = (slot + 1) % new_capacity;

        new_entries[slot] = old_entries[i];
    }

    kfree(old_entries);
    set->entries = new_entries;
    set->capacity = new_capacity;
}

void set32_init(struct set32 *set, size_t capacity) {
    set->entries = (struct set32_entry *)kmalloc(capacity * sizeof(struct set32_entry));
    set->length = 0;
    set->capacity = capacity;

    for (size_t i = 0; i < capacity; i++)
        set->entries[i].state = SET32_SLOT_EMPTY;
}

void set32_destroy(struct set32 *set) {
    kfree(set->entries);

    set->entries = NULL;
    set->length = 0;
    set->capacity = 0;
}

void set32_set(struct set32 *set, uint32_t key, uint32_t value) {
    if (set->length * 10 >= set->capacity * 7)
        _set32_grow(set);

    size_t slot = _set32_hash(key) % set->capacity;
    size_t tombstone_slot = 0;
    bool has_tombstone = false;

    while (set->entries[slot].state != SET32_SLOT_EMPTY) {
        if (set->entries[slot].state == SET32_SLOT_OCCUPIED && set->entries[slot].key == key) {
            set->entries[slot].value = value;
            return;
        }
        if (set->entries[slot].state == SET32_SLOT_TOMBSTONE && !has_tombstone) {
            has_tombstone = true;
            tombstone_slot = slot;
        }
        slot = (slot + 1) % set->capacity;
    }

    if (has_tombstone)
        slot = tombstone_slot;

    set->entries[slot].key = key;
    set->entries[slot].value = value;
    set->entries[slot].state = SET32_SLOT_OCCUPIED;
    set->length++;
}

int set32_get(struct set32 *set, uint32_t key, uint32_t *out) {
    size_t slot = _set32_hash(key) % set->capacity;

    while (set->entries[slot].state != SET32_SLOT_EMPTY) {
        if (set->entries[slot].state == SET32_SLOT_OCCUPIED && set->entries[slot].key == key) {
            *out = set->entries[slot].value;
            return 1;
        }
        slot = (slot + 1) % set->capacity;
    }

    return 0;
}

void set32_remove(struct set32 *set, uint32_t key) {
    size_t slot = _set32_hash(key) % set->capacity;

    while (set->entries[slot].state != SET32_SLOT_EMPTY) {
        if (set->entries[slot].state == SET32_SLOT_OCCUPIED && set->entries[slot].key == key) {
            set->entries[slot].state = SET32_SLOT_TOMBSTONE;
            set->length--;
            return;
        }
        slot = (slot + 1) % set->capacity;
    }
}
