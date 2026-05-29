#pragma once

#define CHAR_BIT sizeof(char)

#define SCHAR_MAX 127
#define SCHAR_MIN (-SCHAR_MAX - 1)
#define UCHAR_MAX 255

/* AArch64: char is unsigned by default */
#define CHAR_MIN 0
#define CHAR_MAX UCHAR_MAX

#define SHRT_MAX 32767
#define SHRT_MIN (-SHRT_MAX - 1)
#define USHRT_MAX 65535

#define INT_MAX 2147483647
#define INT_MIN (-INT_MAX - 1)
#define UINT_MAX 4294967295U

/* LP64: long is 64-bit on AArch64 */
#define LONG_MAX 9223372036854775807L
#define LONG_MIN (-LONG_MAX - 1)
#define ULONG_MAX 18446744073709551615UL

#define LLONG_MAX 9223372036854775807LL
#define LLONG_MIN (-LLONG_MAX - 1)
#define ULLONG_MAX 18446744073709551615ULL