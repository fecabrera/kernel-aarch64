#include <debug.h>
#include <arch/cpu.h>
#include "uchar.h"

char *utf16lencpy(char *dst, const char16_t *src, size_t count)
{
    char *d = dst;
    while ((*d++ = utf16to8(_le16(*src++))))
        count--;
    while (count--)
        *d = '\0';
    return dst;
}

char *utf16bencpy(char *dst, const char16_t *src, size_t count)
{
    char *d = dst;
    while ((*d++ = utf16to8(_be16(*src++))))
        count--;
    while (count--)
        *d = '\0';
    return dst;
}
