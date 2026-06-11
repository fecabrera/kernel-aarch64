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