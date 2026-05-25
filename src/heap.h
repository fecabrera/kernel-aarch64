#ifndef HEAP_H
#define HEAP_H

#include <types.h>
#include <string.h>

struct block_header
{
    size_t size;               // usable bytes (not including header)
    uint32_t free;             // 1 = free, 0 = used
    struct block_header *next; // next block in list (adjacent in memory)
};

#define HEADER_SIZE sizeof(struct block_header)
#define MIN_BLOCK_SIZE 16     // don't create fragments smaller than this
#define HEAP_MAGIC 0xDEADBEEF // used later for overflow detection

#define BLOCK_FREE 1
#define BLOCK_USED 0

extern uint8_t _heap_start[];
extern uint8_t _heap_end[];

void heap_init();

#endif // HEAP_H