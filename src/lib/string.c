#include <string.h>

size_t strlen(const char *s)
{
    size_t n = 0;
    while (*s++)
        n++;
    return n;
}

int memcmp(const void *ptr1, const void *ptr2, size_t num)
{
    const uint8_t *a = ptr1;
    const uint8_t *b = ptr2;
    while (num--)
    {
        if (*a != *b)
            return *a - *b;
        a++;
        b++;
    }
    return 0;
}

void *memcpy(void *destination, const void *source, size_t num)
{
    uint8_t *d = destination;
    const uint8_t *s = source;
    while (num--)
        *d++ = *s++;
    return destination;
}

void *memmove(void *destination, const void *source, size_t num)
{
    uint8_t *d = destination;
    const uint8_t *s = source;
    if (d < s)
        while (num--)
            *d++ = *s++;
    else
    {
        d += num;
        s += num;
        while (num--)
            *--d = *--s;
    }
    return destination;
}

char *strcpy(char *destination, const char *source)
{
    char *d = destination;
    while ((*d++ = *source++))
        ;
    return destination;
}

char *strncpy(char *destination, const char *source, size_t num)
{
    char *d = destination;
    while (num && (*d++ = *source++))
        num--;
    while (num--)
        *d++ = '\0';
    return destination;
}

int strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str1 == *str2)
    {
        str1++;
        str2++;
    }
    return (unsigned char)*str1 - (unsigned char)*str2;
}

int strncmp(const char *str1, const char *str2, size_t num)
{
    while (num-- && *str1 && *str2)
    {
        if (*str1 != *str2)
        {
            return (unsigned char)*str1 - (unsigned char)*str2;
        }
        str1++;
        str2++;
    }
    return 0;
}