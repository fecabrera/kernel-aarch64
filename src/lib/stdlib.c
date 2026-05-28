#include "stdlib.h"

static const char _itoa_digits[] = "0123456789abcdef";

char *itoa(int64_t value, char *str, int base)
{
    char *buff = str;
    int neg = 0;

    if (value == 0)
    {
        *buff++ = '0';
        *buff = '\0';
        return str;
    }

    if (value < 0 && base == 10)
    {
        neg = 1;
        value = -value;
    }

    while (value != 0)
    {
        *buff++ = _itoa_digits[value % base];
        value /= base;
    }

    if (neg)
        *buff++ = '-';

    /* digits were written least-significant first; reverse them */
    char *start = str;
    char *end = buff - 1;
    while (start < end)
    {
        char tmp = *start;
        *start++ = *end;
        *end-- = tmp;
    }

    *buff = '\0';

    return str;
}

int vsprintf(char *str, const char *format, __builtin_va_list args)
{
    char *out = str;
    char tmp[32];

    while (*format)
    {
        if (*format != '%')
        {
            *out++ = *format++;
            continue;
        }

        format++;

        switch (*format++)
        {
        case 'd':
        case 'i':
        {
            int val = __builtin_va_arg(args, int);
            itoa((int64_t)val, tmp, 10);
            for (char *p = tmp; *p; p++)
                *out++ = *p;
            break;
        }
        case 'u':
        {
            unsigned int val = __builtin_va_arg(args, unsigned int);
            itoa((int64_t)val, tmp, 10);
            for (char *p = tmp; *p; p++)
                *out++ = *p;
            break;
        }
        case 'x':
        {
            unsigned int val = __builtin_va_arg(args, unsigned int);
            itoa((int64_t)val, tmp, 16);
            for (char *p = tmp; *p; p++)
                *out++ = *p;
            break;
        }
        case 's':
        {
            char *s = __builtin_va_arg(args, char *);
            while (*s)
                *out++ = *s++;
            break;
        }
        case 'c':
            *out++ = (char)__builtin_va_arg(args, int);
            break;
        case '%':
            *out++ = '%';
            break;
        default:
            *out++ = '%';
            *out++ = *(format - 1);
            break;
        }
    }

    *out = '\0';
    return (int)(out - str);
}

int sprintf(char *str, const char *format, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, format);
    int n = vsprintf(str, format, args);
    __builtin_va_end(args);
    return n;
}