#include "uchar.h"
#include <arch/cpu.h>
#include <debug.h>

char utf16to8(char16_t c) { return c > 0xff ? '?' : (char)(c & 0xff); }

char *utf16lencpy(char *dst, const char16_t *src, size_t count) {
    char *d = dst;
    while (count && (*d++ = utf16to8(_le16(*src++))))
        count--;
    while (count--)
        *d++ = '\0';
    return dst;
}

char *utf16bencpy(char *dst, const char16_t *src, size_t count) {
    char *d = dst;
    while (count && (*d++ = utf16to8(_be16(*src++))))
        count--;
    while (count--)
        *d++ = '\0';
    return dst;
}
