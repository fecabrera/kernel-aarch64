import "debug";
import "dtb";
import "heap";
import "pool";
import "align";

const KERNEL_HEAP_SIZE = 16 * (1 << 20); // 16 MiB heap
const KERNEL_MIN_BLK_SIZE = 16;

const STACK_POOL_SIZE = 16 * (1 << 20); // 16 MiB stack pool
const STACK_POOL_BLK_SIZE = 16 * (1 << 10); // 16 KiB stack blocks;

const DEFAULT_STACK_BLOCK_COUNT = 1;
const DEFAULT_STACK_SIZE = STACK_POOL_BLK_SIZE * DEFAULT_STACK_BLOCK_COUNT;

@static let mem: struct memreg;

@static let kheap_ptr: byte[KERNEL_HEAP_SIZE];
@static let kheap: struct heap;

@static let stack_pool_ptr: byte[STACK_POOL_SIZE];
@static let stack_pool: struct pool;

/**
 * Initializes the kernel heap over the static 16 MiB kheap_ptr region and the process stack pool
 * over the static 16 MiB stack_pool_ptr region (16 KiB blocks), then reads the RAM base address and
 * size from the device tree (DTB) via dtb_get_memory_register and logs them. Must be called after
 * the DTB has been parsed and before any kmalloc/kfree or stack_alloc.
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

/**
 * Registers the kernel handlers for the user-side memory syscalls (ACQMEM/RSZMEM/RELMEM), which
 * back the user build of libmc's malloc/realloc/free. Must be called after syscall_init().
 */
fn register_mem_syscalls() {
    syscall_register_handler(syscall::ACQMEM, syscall_acqmem_handler);
    syscall_register_handler(syscall::RSZMEM, syscall_rszmem_handler);
    syscall_register_handler(syscall::RELMEM, syscall_relmem_handler);
}

/**
 * Allocates `count` contiguous 16 KiB blocks from the stack pool, returning the bottom of the
 * region. Used to back process stacks.
 *
 * @param count: number of 16 KiB blocks to allocate
 *
 * @return pointer to the allocated region, or null if the pool is exhausted
 */
@inline
fn stack_alloc(count: uint64) -> byte* {
    return pool_alloc(&stack_pool, count);
}

/**
 * Returns `count` 16 KiB blocks at `ptr` to the stack pool.
 *
 * @param ptr:   pointer previously returned by stack_alloc
 * @param count: number of blocks the allocation spanned
 */
@inline
fn stack_free(ptr: byte*, count: uint64) {
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
fn kmalloc(size: uint64) -> byte* {
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
fn kmalloc_aligned(size: uint64, align: uint64) -> byte* {
    // Round size up to the next multiple of align. The returned pointer is
    // always (block_header base + HEADER_SIZE); since HEADER_SIZE is a
    // multiple of 8 and every block base is 8-byte aligned, any align that
    // is itself a multiple of 8 (8, 16, 64, 4096, ...) is guaranteed safe
    // with no gap or stored-offset needed. The result can be passed directly
    // to kfree.
    return kmalloc(aligned(size, align));
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
fn krealloc(ptr: byte*, new_size: uint64) -> byte* {
    return heap_resize(&kheap, ptr, new_size);
}

/**
 * Like krealloc, but rounds new_size up to a multiple of align before resizing (every block payload
 * is already 8-byte aligned, so an align that is a power-of-two multiple of 8 needs no extra gap).
 *
 * @param ptr:      pointer returned by kmalloc/kmalloc_aligned, or null
 * @param new_size: desired new size in bytes
 * @param align:    required alignment in bytes; must be a power-of-two multiple of 8
 *
 * @return pointer to the (possibly relocated) memory, or null on failure
 */
@inline
fn krealloc_aligned(ptr: byte*, new_size: uint64, align: uint64) -> byte* {
    let aligned_size = (new_size + align - 1) & ~(align - 1);
    return heap_resize(&kheap, ptr, aligned_size);
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
fn kfree(ptr: byte*) -> int32 {
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
 * Allocates heap space for a single element of type T -- shorthand for
 * alloc<T>(1).
 *
 * @return pointer to the element; the memory is uninitialized
 */
fn knew<T>() -> T* {
    return alloc<T>(1);
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

/**
 * Handles SYSCALL_ACQMEM. Allocates x1 bytes (aligned to x2 if non-zero) from the kernel heap and
 * returns the pointer in x0. Backs the user allocator's malloc/malloc_aligned.
 *
 * @param ctx: saved context; x1 = size, x2 = align (0 for unaligned)
 *
 * @return ctx with x0 set to the allocated pointer (or null on failure)
 */
fn syscall_acqmem_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let size = ctx->x[1] as uint64;
    let align = ctx->x[2] as uint64;

    if (align == 0) ctx->x[0] = kmalloc(size) as uint64;
    else ctx->x[0] = kmalloc_aligned(size, align) as uint64;

    return ctx;
}

/**
 * Handles SYSCALL_RSZMEM. Resizes the block at x1 to x2 bytes (aligned to x3 if non-zero) and
 * returns the (possibly relocated) pointer in x0. Backs the user allocator's realloc.
 *
 * @param ctx: saved context; x1 = ptr, x2 = new size, x3 = align (0 for unaligned)
 *
 * @return ctx with x0 set to the resized pointer (or null on failure)
 */
fn syscall_rszmem_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let ptr = ctx->x[1] as byte*;
    let new_size = ctx->x[2] as uint64;
    let align = ctx->x[3] as uint64;

    if (align == 0) ctx->x[0] = krealloc(ptr, new_size) as uint64;
    else ctx->x[0] = krealloc_aligned(ptr, new_size, align) as uint64;

    return ctx;
}

/**
 * Handles SYSCALL_RELMEM. Frees the block at x1 back to the kernel heap. Backs the user allocator's
 * free.
 *
 * @param ctx: saved context; x1 = ptr to free
 *
 * @return ctx unchanged
 */
fn syscall_relmem_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let ptr = ctx->x[1] as byte*;

    kfree(ptr);

    return ctx;
}
