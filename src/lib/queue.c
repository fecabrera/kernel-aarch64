#include "queue.h"
#include "string.h"

void queue_enqueue(struct queue *q, struct queue_entry *entry)
{
    entry->next = NULL;

    if (q->head == NULL)
    {
        q->head = q->tail = entry;
    }
    else
    {
        q->tail->next = entry;
        q->tail = entry;
    }
}

struct queue_entry *queue_dequeue(struct queue *q)
{
    if (q->head == NULL)
    {
        return NULL;
    }

    struct queue_entry *entry = q->head;
    q->head = entry->next;

    if (q->head == NULL)
    {
        q->tail = NULL;
    }

    return entry;
}