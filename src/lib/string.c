#include <string.h>

size_t strlen(const char *s)
{
    size_t n = 0;
    while (*s++)
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
    while (count-- && *lhs && *rhs)
    {
        if (*lhs != *rhs)
        {
            return (unsigned char)*lhs - (unsigned char)*rhs;
        }
        lhs++;
        rhs++;
    }
    return 0;
}