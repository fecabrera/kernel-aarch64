#pragma once

#include "stdint.h"

typedef int8_t char8_t;
typedef int16_t char16_t;
typedef int32_t char32_t;

// Converts a UTF-16LE code unit to an ASCII byte.
// Returns the low byte for U+0000–U+00FF; '?' for anything above U+00FF.
#define utf16to8(c) ((c > 0xff) ? '?' : (c & 0xff))
