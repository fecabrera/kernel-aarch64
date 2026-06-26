import "system/syscall";

@if (IS_KERNEL) {
    import "mm";

    @inline
    fn malloc(size: uint64) -> uint8* {
        return kmalloc(size);
    }

    @inline
    fn malloc_aligned(size: uint64, align: uint64) -> uint8* {
        return kmalloc_aligned(size, align);
    }
    
    @inline
    fn realloc(ptr: uint8*, new_size: uint64) -> uint8* {
        return krealloc(ptr, new_size);
    }

    @inline    
    fn realloc_aligned(ptr: uint8*, new_size: uint64, align: uint64) -> uint8* {
        return krealloc_aligned(ptr, new_size, align);
    }
    
    @inline
    fn free(ptr: uint8*) {
        kfree(ptr);
    }
}
@else {
    @inline
    fn malloc(size: uint64) -> uint8* {
        return acqmem(size, 0);
    }

    @inline
    fn malloc_aligned(size: uint64, align: uint64) -> uint8* {
        return acqmem(size, align);
    }

    @inline
    fn realloc(ptr: uint8*, new_size: uint64) -> uint8* {
        return rszmem(ptr, new_size, 0);
    }

    @inline
    fn realloc_align(ptr: uint8*, new_size: uint64, align: uint64) -> uint8* {
        return rszmem(ptr, new_size, align);
    }

    @inline
    fn free(ptr: uint8*) {
        relmem(ptr);
    }
}

/**
 * Converts a signed integer to a null-terminated string in the given base. Negative values are only
 * supported for base 10.
 *
 * @param value: integer to convert
 * @param str: output buffer (must be large enough to hold the result)
 * @param base: numeric base (e.g. 2, 8, 10, 16)
 *
 * @return str
 */
@extern fn itoa(value: int64, str: uint8*, base: int32) -> uint8*;

/**
 * Converts an unsigned integer to a null-terminated string in the given base, using lowercase
 * digits. Never emits a sign, so it is correct across the full uint64_t range (e.g. pointers and
 * large addresses in base 16).
 *
 * @param value: unsigned integer to convert
 * @param str: output buffer (must be large enough to hold the result)
 * @param base: numeric base (e.g. 2, 8, 10, 16)
 *
 * @return str
 */
@extern fn utoa(value: uint64, str: uint8*, base: int32) -> uint8*;

/**
 * Converts the initial decimal digits of str to an integer, skipping leading whitespace and
 * handling an optional leading sign. Stops at the first non-digit character.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
@extern fn atoi(str: uint8*) -> int32;

/**
 * Same as atoi but returns long.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
@extern fn atol(str: uint8*) -> int64;

/**
 * Same as atoi but returns long long.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
@extern fn atoll(str: uint8*) -> int64;
