import "debug";
import "dtb";
import "heap";
import "pool";

const KERNEL_HEAP_SIZE = 16 * (1 << 20); // 16 MiB heap
const KERNEL_MIN_BLK_SIZE = 16;

const STACK_POOL_SIZE = 16 * (1 << 20); // 16 MiB stack pool
const STACK_POOL_BLK_SIZE = 16 * (1 << 10); // 16 KiB stack blocks;

const DEFAULT_STACK_BLOCK_COUNT = 1;
const DEFAULT_STACK_SIZE = STACK_POOL_BLK_SIZE * DEFAULT_STACK_BLOCK_COUNT;

@static let mem: struct memreg;

@static let kheap_ptr: uint8[KERNEL_HEAP_SIZE];
@static let kheap: struct heap;

@static let stack_pool_ptr: uint8[STACK_POOL_SIZE];
@static let stack_pool: struct pool;

/**
 * Initializes the kernel heap over the static 16 MiB kheap_ptr region, then reads the RAM base
 * address and size from the device tree (DTB) via dtb_get_memory_register and logs them. Must be
 * called after the DTB has been parsed and before any kmalloc/kfree.
 */
fn mem_init() {
    // initialize kernel heap
    heap_init(&kheap, KERNEL_HEAP_SIZE, KERNEL_MIN_BLK_SIZE, kheap_ptr);

    // initialize stack pool
    pool_init(&stack_pool, stack_pool_ptr as uint64, STACK_POOL_SIZE,
              STACK_POOL_BLK_SIZE);
    pool_dump(&stack_pool);
    
    if (dtb_get_memory_register(&mem) == 0)
        dprintk("[mem] base: %p, size=%i MiB\n", mem.base, mem.size / (1 << 20));
    else
        dprintk("[mem] Memory register not found!");
}

@inline
fn stack_alloc(count: uint64) -> uint8* {
    return pool_alloc(&stack_pool, count);
}

@inline
fn stack_free(ptr: uint8*, count: uint64) {
    pool_free(&stack_pool, ptr, count);
}

/**
 * Allocates size bytes from the kernel heap.
 *
 * @param size: number of bytes to allocate
 *
 * @return pointer to the allocated memory, or null if size is 0 or the heap is exhausted
 */
@inline
fn kmalloc(size: uint64) -> uint8* {
    return heap_acquire(&kheap, size);
}

/**
 * Allocates at least size bytes from the kernel heap with the given alignment, rounding the size up
 * to a multiple of align before allocating (see the note below on why that is sufficient). The
 * result can be passed directly to kfree.
 *
 * @param size:  number of bytes to allocate
 * @param align: required alignment in bytes; must be a power-of-two multiple of 8
 *
 * @return aligned pointer to the allocated memory, or null on failure
 */
@inline
fn kmalloc_aligned(size: uint64, align: uint64) -> uint8* {
    // Round size up to the next multiple of align. The returned pointer is
    // always (block_header base + HEADER_SIZE); since HEADER_SIZE is a
    // multiple of 8 and every block base is 8-byte aligned, any align that
    // is itself a multiple of 8 (8, 16, 64, 4096, ...) is guaranteed safe
    // with no gap or stored-offset needed. The result can be passed directly
    // to kfree.
    let aligned_size = (size + align - 1) & ~(align - 1);
    return kmalloc(aligned_size);
}

/**
 * Resizes a kernel-heap block, preserving its contents up to the smaller of the old and new sizes.
 * A null ptr behaves like kmalloc; a new_size of 0 frees ptr and returns null.
 *
 * @param ptr:      pointer returned by kmalloc, or null
 * @param new_size: desired new size in bytes
 *
 * @return pointer to the (possibly relocated) memory, or null on failure
 */
@inline
fn krealloc(ptr: uint8*, new_size: uint64) -> uint8* {
    return heap_resize(&kheap, ptr, new_size);
}

/**
 * Frees a block previously returned by kmalloc/kmalloc_aligned/krealloc back to the kernel heap.
 *
 * @param ptr: pointer to free
 *
 * @return 0 on success, or a negative heap_release error code (-1 null, -2 out of range, -3 double
 *         free)
 */
@inline
fn kfree(ptr: uint8*) -> int32 {
    return heap_release(&kheap, ptr);
}

/**
 * Allocates heap space for n elements of type T.
 *
 * @param n: number of elements to allocate space for
 *
 * @return pointer to the first element; the memory is uninitialized
 */
fn kalloc<T>(n: uint64) -> T* {
    return kmalloc(n * sizeof(T)) as T*;
}

/**
 * Allocates heap space for n elements of type T with the given alignment.
 * The result is directly kdealloc-able. align must be a power-of-two multiple
 * of 8.
 *
 * @param n:     number of elements to allocate space for
 * @param align: required alignment in bytes
 *
 * @return pointer to the first element; the memory is uninitialized
 */
fn kalloc_aligned<T>(n: uint64, align: uint64) -> T* {
    return kmalloc_aligned(n * sizeof(T), align) as T*;
}

/**
 * Resizes a block previously returned by kalloc<T> to hold n elements of type
 * T, preserving its contents up to the smaller of the old and new sizes.
 * Returns the (possibly relocated) pointer; the old pointer must not be used
 * afterward. Passing null allocates a fresh block.
 *
 * @param p: pointer returned by kalloc<T>, or null
 * @param n: new number of elements
 *
 * @return pointer to the resized block
 */
@inline
fn kresize<T>(p: T*, n: uint64) -> T* {
    return krealloc(p, n * sizeof(T)) as T*;
}

/**
 * Releases memory previously returned by kalloc.
 *
 * @param p: pointer returned by kalloc<T>; null is allowed and does nothing
 */
@inline
fn kdealloc<T>(p: T*) {
    kfree(p);
}
