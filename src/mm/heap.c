#include "heap.h"
#include <arch/cpu.h>
#include <debug.h>
#include <stdint.h>

static struct block_header *heap_head = 0;

uint8_t _heap[HEAP_SIZE] = {0};

void heap_init() {
    heap_head = (struct block_header *)_heap;
    heap_head->size = (size_t)(_heap + HEAP_SIZE - _heap) - HEADER_SIZE;
    heap_head->free = BLOCK_FREE;
    heap_head->next = NULL;

    dprintk("[heap] heap initialized: start=0x%08X, size=%d B\n", _heap, heap_head->size);
}

// Merge adjacent free blocks (called after kfree, prevents fragmentation)
void merge_free_blocks() {
    struct block_header *cur = heap_head;
    while (cur && cur->next) {
        if (cur->free && cur->next->free) {
            // Absorb next block into current
            cur->size += HEADER_SIZE + cur->next->size;
            cur->next = cur->next->next;
            // Don't advance — merged block might be mergeable again
        } else {
            cur = cur->next;
        }
    }
}

void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }

    // Align size to 8 bytes
    size = (size + 7) & ~7UL;

    struct block_header *cur = heap_head;

    while (cur) {
        if (cur->free && cur->size >= size) {
            // Split block if remainder is large enough to be useful
            if (cur->size >= size + HEADER_SIZE + MIN_BLOCK_SIZE) {
                struct block_header *new_block =
                    (struct block_header *)((uint8_t *)cur + HEADER_SIZE + size);
                new_block->size = cur->size - size - HEADER_SIZE;
                new_block->free = BLOCK_FREE;
                new_block->next = cur->next;

                cur->size = size;
                cur->next = new_block;
            }

            cur->free = BLOCK_USED;
            uint8_t *ptr = (uint8_t *)cur + HEADER_SIZE;

            if (ptr >= _heap + HEAP_SIZE) {
                printk("address=0x%016X\n", ptr);
                halt();
            }

            return (void *)ptr;
        }
        cur = cur->next;
    }

    printk("[heap] kmalloc: out of memory!\n");

    struct block_header *_head = heap_head;
    while (_head) {
        printk("block at 0x%x: size=%d B %s\n", _head + HEADER_SIZE, _head->size,
               _head->free ? "FREE" : "USED");
        _head = _head->next;
    }

    return NULL;
}

int kfree(void *ptr) {
    if (!ptr) {
        return -1;
    }

    struct block_header *hdr = (struct block_header *)((uint8_t *)ptr - HEADER_SIZE);

    // Sanity check — ptr should be inside heap
    if ((uint8_t *)hdr < _heap || (uint8_t *)hdr >= _heap + HEAP_SIZE) {
        printk("[heap] kfree: pointer out of range!\n");
        return -2;
    }

    if (hdr->free) {
        printk("[heap] kfree: double free detected!\n");
        return -3;
    }

    hdr->free = BLOCK_FREE;
    merge_free_blocks();
    return 0;
}

void *krealloc(void *ptr, size_t new_size) {
    if (!ptr) {
        return kmalloc(new_size);
    }

    if (!new_size) {
        kfree(ptr);
        return 0;
    }

    struct block_header *hdr = (struct block_header *)((uint8_t *)ptr - HEADER_SIZE);

    // Already big enough (and not wasteful to keep)
    if (hdr->size >= new_size && hdr->size <= new_size * 2)
        return ptr;

    void *new_ptr = kmalloc(new_size);
    if (!new_ptr) {
        return 0;
    }

    size_t copy_size = hdr->size < new_size ? hdr->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    kfree(ptr);
    return new_ptr;
}

void *kmalloc_aligned(size_t size, size_t align) {
    // Round size up to the next multiple of align. The returned pointer is
    // always (block_header base + HEADER_SIZE); since HEADER_SIZE is a
    // multiple of 8 and every block base is 8-byte aligned, any align that
    // is itself a multiple of 8 (8, 16, 64, 4096, ...) is guaranteed safe
    // with no gap or stored-offset needed. The result can be passed directly
    // to kfree.
    size_t aligned_size = (size + align - 1) & ~(align - 1);
    return kmalloc(aligned_size);
}

void heap_dump() {
    dprintk("\n=== heap dump ===\n");

    struct block_header *cur = heap_head;
    size_t used = 0, free = 0;

    int i = 0;
    while (cur) {
        dprintk("  [%i] addr=0x%x size=%i %s\n", i++, cur + HEADER_SIZE, cur->size,
                cur->free ? "FREE" : "USED");

        if (cur->free)
            free += cur->size;
        else
            used += cur->size;

        cur = cur->next;
    }

    dprintk("  used: %i bytes\n", used);
    dprintk("  free: %i bytes\n", free);
    dprintk("=================\n\n");
}