const STB_SPRINTF_MIN = 512;

struct stbsp__context {
    buf: uint8*;
    count: int32;
    length: int32;
    tmp: uint8[STB_SPRINTF_MIN];
}

/**
 * Formats a string into str using a printf-style format and a pre-initialized va_list.
 * Supports: %d/%i (signed int), %u (unsigned int), %x (lowercase hex), %X (uppercase hex),
 * %s (string), %c (char), %%. Width modifier N supported for all specifiers: %Nd space-pads on
 * the left (e.g. %8d), %0Nd zero-pads (e.g. %08x). For strings, %-Ns left-aligns within width N
 * (padding on the right); %Ns right-aligns (padding on the left).
 *
 * @param str:    output buffer
 * @param format: printf-style format string
 * @param args:   variadic argument list (must be initialized by the caller)
 *
 * @return number of characters written, not including the null terminator
 */
@extern fn vsprintf(str: uint8*, format: uint8*, args: va_list) -> int32;
@extern fn vsnprintf(str: uint8*, count: int32, format: uint8*, args: va_list) -> int32;

/**
 * Formats a string into str using a printf-style format.
 * Supports: %d/%i (signed int), %u (unsigned int), %x (lowercase hex), %X (uppercase hex),
 * %s (string), %c (char), %%. Width modifier N supported for all specifiers: %Nd space-pads on
 * the left (e.g. %8d), %0Nd zero-pads (e.g. %08x). For strings, %-Ns left-aligns within width N
 * (padding on the right); %Ns right-aligns (padding on the left).
 *
 * @param str:    output buffer
 * @param format: printf-style format string
 * @param ...:    variadic arguments matching the format specifiers
 *
 * @return number of characters written, not including the null terminator
 */
@extern fn sprintf(str: uint8*, format: uint8*, ...) -> int32;
@extern fn snprintf(str: uint8*, count: int32, format: uint8*, ...) -> int32;

@extern fn vsprintfcb(callback: fn (uint8*, struct stbsp__context*, int32) -> uint8*,
                      user: struct stbsp__context*, str: uint8*,
                      format: uint8*, args: va_list) -> int32;