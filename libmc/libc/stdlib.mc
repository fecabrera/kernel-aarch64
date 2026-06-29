@if (IS_KERNEL) {
    import "mm";

    @inline
    fn malloc(size: uint64) -> byte* {
        return kmalloc(size);
    }

    @inline
    fn malloc_aligned(size: uint64, align: uint64) -> byte* {
        return kmalloc_aligned(size, align);
    }
    
    @inline
    fn realloc(ptr: byte*, new_size: uint64) -> byte* {
        return krealloc(ptr, new_size);
    }

    @inline    
    fn realloc_aligned(ptr: byte*, new_size: uint64, align: uint64) -> byte* {
        return krealloc_aligned(ptr, new_size, align);
    }
    
    @inline
    fn free(ptr: byte*) {
        kfree(ptr);
    }
}
@else {
    import "system/syscall";
    
    @inline
    fn malloc(size: uint64) -> byte* {
        return acqmem(size, 0);
    }

    @inline
    fn malloc_aligned(size: uint64, align: uint64) -> byte* {
        return acqmem(size, align);
    }

    @inline
    fn realloc(ptr: byte*, new_size: uint64) -> byte* {
        return rszmem(ptr, new_size, 0);
    }

    @inline
    fn realloc_align(ptr: byte*, new_size: uint64, align: uint64) -> byte* {
        return rszmem(ptr, new_size, align);
    }

    @inline
    fn free(ptr: byte*) {
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
@extern fn itoa(value: int64, str: char*, base: int32) -> char*;

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
@extern fn utoa(value: uint64, str: char*, base: int32) -> char*;

/**
 * Converts the initial decimal digits of str to an integer, skipping leading whitespace and
 * handling an optional leading sign. Stops at the first non-digit character.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
@extern fn atoi(str: char*) -> int32;

/**
 * Same as atoi but returns long.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
@extern fn atol(str: char*) -> int64;

/**
 * Same as atoi but returns long long.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
@extern fn atoll(str: char*) -> int64;
