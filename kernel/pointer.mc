import "debug";
import "mm";

/**
 * A reference-counted owner of a heap-allocated T. Shared owners hold the same
 * pointer<T> and bump/drop count via pointer_acquire/pointer_release; the value
 * and the pointer itself are freed when count reaches zero. Not IRQ/thread-safe:
 * count updates are plain (non-atomic) read-modify-writes.
 *
 * @field value: the owned, heap-allocated T
 * @field count: number of live owners
 */
struct pointer<T> {
    value: T*;
    count: uint64;
}

/**
 * Allocates a pointer<T> and its value with an initial reference count of 1.
 *
 * @return the new pointer<T> (caller is the first owner)
 */
fn create_pointer<T>() -> struct pointer<T>* {
    let ptr: struct pointer<T>* = kalloc<struct pointer<T>>(1);
    ptr->value = kalloc<T>(1);
    ptr->count = 1;
    dprintk("[ptr] created pointer at %p, count = %llu\n", ptr->value, ptr->count);
    return ptr;
}

/**
 * Adds a reference: increments the count and returns the same pointer. Null-safe.
 *
 * @param ptr: pointer to acquire, or null
 *
 * @return ptr (or null if ptr was null)
 */
fn pointer_acquire<T>(ptr: struct pointer<T>*) -> struct pointer<T>* {
    if (ptr == null)
        return null;
    ptr->count = ptr->count + 1;
    dprintk("[ptr] aquired pointer at %p, count = %llu\n", ptr->value, ptr->count);
    return ptr;
}

/**
 * Drops a reference: decrements the count and, when it reaches zero, frees both
 * the value and the pointer<T>. Null-safe.
 *
 * @param ptr: pointer to release, or null
 */
fn pointer_release<T>(ptr: struct pointer<T>*) {
    if (ptr == null)
        return;
    ptr->count = ptr->count - 1;
    dprintk("[ptr] released pointer at %p, count = %llu\n", ptr->value, ptr->count);
    if (ptr->count == 0) {
        dprintk("[ptr] pointer at %p has skedaddled itself\n", ptr->value);
        kdealloc(ptr->value);
        kdealloc(ptr);
    }
}