#include <drivers/uart.h>
#include "heap.h"

static struct block_header *heap_head = 0;

extern uint8_t _heap_start[];
extern uint8_t _heap_end[];

void heap_init()
{
    heap_head = (struct block_header *)_heap_start;
    heap_head->size = (size_t)(_heap_end - _heap_start) - HEADER_SIZE;
    heap_head->free = BLOCK_FREE;
    heap_head->next = NULL;
}

// Merge adjacent free blocks (called after kfree, prevents fragmentation)
void merge_free_blocks()
{
    struct block_header *cur = heap_head;
    while (cur && cur->next)
    {
        if (cur->free && cur->next->free)
        {
            // Absorb next block into current
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next = cur->next->next;
            // Don't advance — merged block might be mergeable again
        }
        else
        {
            cur = cur->next;
        }
    }
}

void *kmalloc(size_t size)
{
    if (size == 0)
    {
        return NULL;
    }

    // Align size to 8 bytes
    size = (size + 7) & ~7UL;

    struct block_header *cur = heap_head;

    while (cur)
    {
        if (cur->free && cur->size >= size)
        {
            // Split block if remainder is large enough to be useful
            if (cur->size >= size + HEADER_SIZE + MIN_BLOCK_SIZE)
            {
                struct block_header *new_block = (struct block_header *)((uint8_t *)cur + HEADER_SIZE + size);
                new_block->size = cur->size - size - HEADER_SIZE;
                new_block->free = 1;
                new_block->next = cur->next;

                cur->size = size;
                cur->next = new_block;
            }

            cur->free = 0;
            return (void *)((uint8_t *)cur + HEADER_SIZE);
        }
        cur = cur->next;
    }

    uart_puts("[heap] kmalloc: out of memory!\r\n");
    return NULL;
}

void kfree(void *ptr)
{
    if (!ptr)
    {
        return;
    }

    struct block_header *hdr = (struct block_header *)((uint8_t *)ptr - HEADER_SIZE);

    // Sanity check — ptr should be inside heap
    if ((uint8_t *)hdr < _heap_start || (uint8_t *)hdr >= _heap_end)
    {
        uart_puts("[heap] kfree: pointer out of range!\r\n");
        return;
    }

    if (hdr->free)
    {
        uart_puts("[heap] kfree: double free detected!\r\n");
        return;
    }

    hdr->free = 1;
    merge_free_blocks();
}

void *krealloc(void *ptr, size_t new_size)
{
    if (!ptr)
    {
        return kmalloc(new_size);
    }

    if (!new_size)
    {
        kfree(ptr);
        return 0;
    }

    struct block_header *hdr = (struct block_header *)((uint8_t *)ptr - HEADER_SIZE);

    // Already big enough (and not wasteful to keep)
    if (hdr->size >= new_size && hdr->size <= new_size * 2)
        return ptr;

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr)
    {
        return 0;
    }

    size_t copy_size = hdr->size < new_size ? hdr->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    kfree(ptr);
    return new_ptr;
}

void *kmalloc_aligned(size_t size, size_t align)
{
    // Allocate extra space to guarantee we can align within it
    void *raw = kmalloc(size + align);
    if (!raw)
    {
        return 0;
    }

    uintptr_t addr = (uintptr_t)raw;
    uintptr_t aligned = (addr + align - 1) & ~(align - 1);

    // If already aligned, we're done
    if (aligned == addr)
        return raw;

    // Otherwise we've wasted the gap — acceptable for now
    // (a real allocator would store the offset to allow kfree)
    return (void *)aligned;
}

void heap_dump()
{
    uart_puts("\r\n=== heap dump ===\r\n");

    struct block_header *cur = heap_head;
    size_t used = 0, free = 0;

    int i = 0;
    while (cur)
    {
        uart_puts("  [");
        uart_put_uint(i++);
        uart_puts("] addr=0x");
        uart_put_uint_hex((uintptr_t)cur + HEADER_SIZE);
        uart_puts(" size=");
        uart_put_uint(cur->size);
        uart_puts(cur->free ? " FREE" : " USED");
        uart_puts("\r\n");

        if (cur->free)
            free += cur->size;
        else
            used += cur->size;

        cur = cur->next;
    }

    uart_puts("  used: ");
    uart_put_uint(used);
    uart_puts(" bytes\r\n");
    uart_puts("  free: ");
    uart_put_uint(free);
    uart_puts(" bytes\r\n");
    uart_puts("=================\r\n\r\n");
}