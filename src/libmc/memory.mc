import "mm/heap";
import "libc/string";

/**
 * Allocates heap space for n elements of type T.
 *
 * @param n: number of elements to allocate space for
 *
 * @return pointer to the first element; the memory is uninitialized
 */
fn alloc<T>(n: uint64) -> T* {
    return kmalloc(n * sizeof(T)) as T*;
}

/**
 * Allocates heap space for n elements of type T with the given alignment.
 * The result is directly dealloc-able. align must be a power-of-two multiple
 * of 8.
 *
 * @param n:     number of elements to allocate space for
 * @param align: required alignment in bytes
 *
 * @return pointer to the first element; the memory is uninitialized
 */
fn alloc_aligned<T>(n: uint64, align: uint64) -> T* {
    return kmalloc_aligned(n * sizeof(T), align) as T*;
}

/**
 * Releases memory previously returned by alloc.
 *
 * @param p: pointer returned by alloc<T>; null is allowed and does nothing
 */
fn dealloc<T>(p: T*) {
    kfree(p);
}

/**
 * Byte-copies n elements of type T from src to dst in a single memcpy.
 * The element size is computed from T, so callers count elements, not
 * bytes. The regions must not overlap.
 *
 * @param dst: destination, with room for at least n elements
 * @param src: source to read from
 * @param n:   number of elements to copy
 */
fn copy_bytes<T>(dst: T*, src: T*, n: uint64) {
    memcpy(dst, src, n * sizeof(T));
}

/**
 * Fills the n elements of type T at dst with the byte `value`, in a single
 * memset. The element size is computed from T, so callers count elements,
 * not bytes; pass 0 to zero the region.
 *
 * @param dst:   destination, with room for at least n elements
 * @param value: the byte written to every byte of the region
 * @param n:     number of elements to fill
 */
fn set_bytes<T>(dst: T*, value: uint8, n: uint64) {
    memset(dst, value as int32, n * sizeof(T));   // libc memset takes an int
}

/**
 * Copies n elements of type T from src to dst one item at a time.
 *
 * @param dst: destination, with room for at least n elements
 * @param src: source to read from
 * @param n:   number of elements to copy
 */
fn copy_items<T>(dst: T*, src: T*, n: uint64) {
    let i: uint64 = 0;
    while (i < n) {
        defer i = i + 1;
        dst[i] = src[i];
    }
}

/**
 * Sets the n elements of type T at dst to `value`, one item at a time -- so
 * it writes whole T values, not just a repeated byte pattern.
 *
 * @param dst:   destination, with room for at least n elements
 * @param value: the value written to each element
 * @param n:     number of elements to set
 */
fn set_items<T>(dst: T*, value: T, n: uint64) {
    let i: uint64 = 0;
    while (i < n) {
        defer i = i + 1;
        dst[i] = value;
    }
}