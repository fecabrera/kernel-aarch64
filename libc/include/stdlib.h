#pragma once

#include <stdint.h>

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
char *itoa(int64_t value, char *str, int base);

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
char *utoa(uint64_t value, char *str, int base);

/**
 * Converts the initial decimal digits of str to an integer, skipping leading whitespace and
 * handling an optional leading sign. Stops at the first non-digit character.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
int atoi(const char *str);

/**
 * Same as atoi but returns long.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
long atol(const char *str);

/**
 * Same as atoi but returns long long.
 *
 * @param str: null-terminated string to parse
 *
 * @return parsed value; 0 if no digits are found
 */
long long atoll(const char *str);