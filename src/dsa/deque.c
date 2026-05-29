#include <mm/heap.h>
#include "deque.h"

static void _deque64_insert_entry(struct deque64_entry *entry, struct deque64_entry *prev, struct deque64_entry *next)
{
    entry->next = next;
    entry->prev = prev;

    if (prev)
        prev->next = entry;

    if (next)
        next->prev = entry;
}

static void _deque64_remove_entry(struct deque64_entry *entry)
{
    struct deque64_entry *prev = entry->prev;
    struct deque64_entry *next = entry->next;

    if (prev)
        prev->next = next;

    if (next)
        next->prev = prev;
}

void deque64_add_left(struct deque64 *dq, uint64_t value)
{
    struct deque64_entry *entry = (struct deque64_entry *)kmalloc(sizeof(struct deque64_entry));
    entry->value = value;

    _deque64_insert_entry(entry, NULL, dq->head);

    dq->head = entry;

    if (dq->tail == NULL)
        dq->tail = entry;
}

void deque64_add_right(struct deque64 *dq, uint64_t value)
{
    struct deque64_entry *entry = (struct deque64_entry *)kmalloc(sizeof(struct deque64_entry));
    entry->value = value;

    _deque64_insert_entry(entry, dq->tail, NULL);

    dq->tail = entry;

    if (dq->head == NULL)
        dq->head = entry;
}

uint64_t deque64_remove_left(struct deque64 *dq)
{
    struct deque64_entry *entry = dq->head;
    uint64_t value = entry->value;

    dq->head = entry->next;
    if (dq->tail == entry)
        dq->tail = NULL;

    _deque64_remove_entry(entry);
    kfree(entry);

    return value;
}

uint64_t deque64_remove_right(struct deque64 *dq)
{
    struct deque64_entry *entry = dq->tail;
    uint64_t value = entry->value;

    dq->tail = entry->prev;
    if (dq->head == entry)
        dq->head = NULL;

    _deque64_remove_entry(entry);
    kfree(entry);
    return value;
}

uint64_t deque64_peek_left(struct deque64 *dq)
{
    return dq->head->value;
}

uint64_t deque64_peek_right(struct deque64 *dq)
{
    return dq->tail->value;
}

int deque64_is_empty(struct deque64 *dq)
{
    return dq->head == NULL;
}

struct deque64_entry *deque64_remove(struct deque64 *dq, struct deque64_entry *entry)
{
    if (entry == NULL)
        return NULL;

    if (entry == dq->head)
        dq->head = entry->next;

    if (entry == dq->tail)
        dq->tail = entry->prev;

    _deque64_remove_entry(entry);

    return entry;
}

struct deque64_entry *deque64_find(struct deque64 *dq, struct deque64_entry *start, int (*cmp)(struct deque64_entry *, void *), void *ctx)
{
    struct deque64_entry *entry = NULL;

    while ((entry = deque64_next(dq, start)))
    {
        if (cmp(entry, ctx) == 0)
            return entry;
    }

    return NULL;
}

struct deque64_entry *deque64_find_remove(struct deque64 *dq, struct deque64_entry *start, int (*cmp)(struct deque64_entry *, void *), void *ctx)
{
    return deque64_remove(dq, deque64_find(dq, start, cmp, ctx));
}

struct deque64_entry *deque64_next(struct deque64 *dq, struct deque64_entry *start)
{
    return start ? start->next : dq->head;
}
