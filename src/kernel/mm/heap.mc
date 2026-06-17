/**
 * Initializes the heap allocator. Sets up the entire heap region as a single free block. Must be
 * called before any kmalloc/kfree calls.
 */
@extern fn heap_init();

/**
 * Scans the free list and merges adjacent free blocks into one. Reduces fragmentation after kfree
 * calls.
 */
@extern fn merge_free_blocks();

/**
 * Allocates size bytes from the heap. The returned pointer is 8-byte aligned. Splits the chosen
 * block if the remainder is large enough to be reused.
 *
 * @param size: number of bytes to allocate
 *
 * @return pointer to allocated memory, or NULL if the heap is exhausted or size is 0.
 */
@extern fn kmalloc(size: uint64) -> uint8*;

/**
 * Frees a previously allocated block and merges adjacent free blocks.
 *
 * @param ptr: pointer returned by kmalloc or kmalloc_aligned
 *
 * @return  0 on success
 *         -1 if ptr is NULL
 *         -2 if ptr is outside the heap region
 *         -3 if the block was already free (double-free detected)
 */
@extern fn kfree(ptr: uint8*) -> int32;

/**
 * Resizes a previously allocated block. Allocates a new block, copies the smaller of the old and
 * new sizes, then frees the original.
 *
 * @param ptr: pointer returned by kmalloc, or NULL to behave like kmalloc
 * @param new_size: desired new size in bytes
 *
 * @return pointer to resized memory, or NULL on allocation failure.
 */
@extern fn krealloc(ptr: uint8*, new_size: uint64) -> uint8*;

/**
 * Allocates at least size bytes from the heap with the given alignment. Rounds size up to the next
 * multiple of align before calling kmalloc. Safe for any align that is a multiple of 8 (e.g. 8, 16,
 * 64, 4096) — because HEADER_SIZE is a multiple of 8 and every block base is 8-byte aligned, the
 * payload address inherits the alignment without any gap or stored offset. The returned pointer can
 * be passed directly to kfree.
 *
 * @param size:  number of bytes to allocate
 * @param align: required alignment; must be a power of two and a multiple of 8
 *
 * @return aligned pointer to allocated memory, or NULL on failure.
 */
@extern fn kmalloc_aligned(size: uint64, align: uint64) -> uint8*;

/**
 * Prints a summary of every block in the heap (address, size, free/used) and total used/free byte
 * counts to the UART.
 */
@extern fn heap_dump();
