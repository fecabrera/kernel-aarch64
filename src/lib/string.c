#include <string.h>
#include <ctype.h>

size_t strlen(const char *s)
{
    size_t n = 0;
    while (*s++)
        n++;
    return n;
}

size_t strnlen(const char *str, size_t count)
{
    size_t n = 0;
    while (n < count && *str++)
        n++;
    return n;
}

int memcmp(const void *lhs, const void *rhs, size_t count)
{
    const uint8_t *a = lhs;
    const uint8_t *b = rhs;
    while (count--)
    {
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    return 0;
}

void *memcpy(void *dest, const void *source, size_t count)
{
    uint8_t *d = dest;
    const uint8_t *s = source;
    while (count--)
        *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *source, size_t count)
{
    uint8_t *d = dest;
    const uint8_t *s = source;
    if (d < s)
        while (count--)
            *d++ = *s++;
    else
    {
        d += count;
        s += count;
        while (count--)
            *--d = *--s;
    }
    return dest;
}

char *strcpy(char *dest, const char *source)
{
    char *d = dest;
    while ((*d++ = *source++))
        ;
    return dest;
}

char *strncpy(char *dest, const char *source, size_t count)
{
    char *d = dest;
    while (count && (*d++ = *source++))
        count--;
    while (count--)
        *d++ = '\0';
    return dest;
}

size_t strntrimend(char *dest, const char *src, size_t count)
{
    int last_char_idx = -1;
    for (size_t i = 0; i < count; i++)
    {
        char ch = src[i];
        if (isprint(ch) && !isspace(ch))
            last_char_idx = i;
    }

    if (last_char_idx < 0)
    {
        dest[0] = '\0';
        return 0;
    }

    strncpy(dest, src, last_char_idx + 1);
    dest[last_char_idx + 1] = '\0';

    return last_char_idx + 1;
}

void *memset(void *dest, int ch, size_t count)
{
    uint8_t *d = dest;
    while (count--)
        *d++ = (uint8_t)ch;
    return dest;
}

int strcmp(const char *lhs, const char *rhs)
{
    while (*lhs && *lhs == *rhs)
    {
        lhs++;
        rhs++;
    }
    return (unsigned char)*lhs - (unsigned char)*rhs;
}

int strncmp(const char *lhs, const char *rhs, size_t count)
{
    while (count--)
    {
        unsigned char a = (unsigned char)*lhs;
        unsigned char b = (unsigned char)*rhs;
        if (a != b)
        {
            return a - b;
        }
        if (a == '\0')
        {
            return 0;
        }
        lhs++;
        rhs++;
    }
    return 0;
}