#pragma once

#include <stdint.h>

/**
 * Converts a signed integer to a null-terminated string in the given base.
 * Negative values are only supported for base 10.
 *
 * @param value: integer to convert
 * @param str: output buffer (must be large enough to hold the result)
 * @param base: numeric base (e.g. 2, 8, 10, 16)
 *
 * @return str
 */
char *itoa(int64_t value, char *str, int base);

/**
 * Formats a string into str using a printf-style format and a pre-initialized
 * va_list. Supports: %d/%i (signed int), %u (unsigned int), %x (hex),
 * %s (string), %c (char), %%.
 *
 * @param str: output buffer
 * @param format: printf-style format string
 * @param args: variadic argument list (must be initialized by the caller)
 *
 * @return number of characters written, not including the null terminator
 */
int vsprintf(char *str, const char *format, __builtin_va_list args);

/**
 * Formats a string into str using a printf-style format.
 * Supports: %d/%i (signed int), %u (unsigned int), %x (hex),
 * %s (string), %c (char), %%.
 *
 * @param str: output buffer
 * @param format: printf-style format string
 * @param ...: variadic arguments matching the format specifiers
 *
 * @return number of characters written, not including the null terminator
 */
int sprintf(char *str, const char *format, ...);