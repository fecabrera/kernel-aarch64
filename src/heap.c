#include "heap.h"

static struct block_header *heap_head = 0;

void heap_init()
{
    heap_head = (struct block_header *)_heap_start;
    heap_head->size = (size_t)(_heap_end - _heap_start) - HEADER_SIZE;
    heap_head->free = BLOCK_FREE;
    heap_head->next = NULL;
}