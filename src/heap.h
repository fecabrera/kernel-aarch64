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

/**
 * Initializes the heap allocator. Sets up the entire heap region as a single
 * free block. Must be called before any kmalloc/kfree calls.
 */
void heap_init();

/**
 * Scans the free list and merges adjacent free blocks into one.
 * Reduces fragmentation after kfree calls.
 */
void merge_free_blocks();

/**
 * Allocates size bytes from the heap. The returned pointer is 8-byte aligned.
 * Splits the chosen block if the remainder is large enough to be reused.
 *
 * @param size: number of bytes to allocate
 *
 * @return pointer to allocated memory, or NULL if the heap is exhausted or size is 0.
 */
void *kmalloc(size_t size);

/**
 * Frees a previously allocated block and merges adjacent free blocks.
 * Passing NULL is a no-op.
 *
 * @param ptr: pointer returned by kmalloc or krealloc
 */
void kfree(void *ptr);

/**
 * Resizes a previously allocated block. Allocates a new block, copies the
 * smaller of the old and new sizes, then frees the original.
 *
 * @param ptr: pointer returned by kmalloc, or NULL to behave like kmalloc
 * @param new_size: desired new size in bytes
 *
 * @return pointer to resized memory, or NULL on allocation failure.
 */
void *krealloc(void *ptr, size_t new_size);

/**
 * Allocates size bytes aligned to a power-of-two boundary.
 * Allocates extra space internally to guarantee alignment; the gap is not
 * recoverable by kfree (acceptable for long-lived aligned allocations).
 *
 * @param size: number of bytes to allocate
 * @param align: required alignment (must be a power of two)
 *
 * @return aligned pointer to allocated memory, or NULL on failure.
 */
void *kmalloc_aligned(size_t size, size_t align);

/**
 * Prints a summary of every block in the heap (address, size, free/used)
 * and total used/free byte counts to the UART.
 */
void heap_dump();

#endif // HEAP_H