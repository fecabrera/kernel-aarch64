#pragma once

#include <stdint.h>
#include <string.h>

typedef int8_t char8_t;
typedef int16_t char16_t;
typedef int32_t char32_t;

/**
 * Converts a UTF-16 code unit to an ASCII byte.
 * Returns the low byte for code points U+0000–U+00FF; '?' for anything above.
 *
 * @param c: UTF-16 code unit to convert
 *
 * @return ASCII byte, or '?' if c is outside the Latin-1 range
 */
char utf16to8(char16_t c);

/**
 * Copies up to n little-endian UTF-16 code units from src into dst as ASCII
 * bytes, applying _le16 byte-swap before conversion. Stops at null terminator.
 * Always null-terminates dst. Non-ASCII code points become '?'.
 *
 * @param dst: output buffer (must hold at least n+1 bytes)
 * @param src: UTF-16LE source array
 * @param n:   maximum number of code units to convert
 *
 * @return dst
 */
char *utf16lencpy(char *dst, const char16_t *src, size_t n);

/**
 * Copies up to n big-endian UTF-16 code units from src into dst as ASCII
 * bytes, applying _be16 byte-swap before conversion. Stops at null terminator.
 * Always null-terminates dst. Non-ASCII code points become '?'.
 *
 * @param dst: output buffer (must hold at least n+1 bytes)
 * @param src: UTF-16BE source array
 * @param n:   maximum number of code units to convert
 *
 * @return dst
 */
char *utf16bencpy(char *dst, const char16_t *src, size_t n);
