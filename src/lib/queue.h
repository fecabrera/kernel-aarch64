#ifndef QUEUE_H
#define QUEUE_H

// Intrusive singly-linked FIFO. Callers allocate queue_entry nodes (e.g. via
// kmalloc) and embed their payload in the data pointer. The queue itself holds
// head/tail pointers and does no allocation of its own.

struct queue_entry
{
    void *data;
    struct queue_entry *next;
};

struct queue
{
    struct queue_entry *head;
    struct queue_entry *tail;
};

/**
 * Appends entry to the tail of q. Sets entry->next to NULL.
 *
 * @param q:     queue to append to
 * @param entry: node to append; must not already be in a queue
 */
void queue_enqueue(struct queue *q, struct queue_entry *entry);

/**
 * Removes and returns the head of q. Updates q->tail to NULL when the last
 * entry is removed.
 *
 * @param q: queue to dequeue from
 *
 * @return the former head entry, or NULL if q is empty.
 */
struct queue_entry *queue_dequeue(struct queue *q);

#endif // QUEUE_H