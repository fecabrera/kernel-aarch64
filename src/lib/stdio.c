#include "stdio.h"
#include "stdlib.h"
#include <ctype.h>
#include <stdbool.h>

int vsprintf(char *str, const char *format, __builtin_va_list args) {
    char *out = str;
    char tmp[32];

    while (*format) {
        if (*format != '%') {
            *out++ = *format++;
            continue;
        }

        format++;

        // parse optional flags/width: %[-][0]<width><conv> e.g. %-8s, %08x
        bool left_align = false;
        char pad_char = ' ';
        int pad_width = 0;
        if (*format == '-') {
            left_align = true;
            format++;
        }
        if (*format == '0') {
            pad_char = '0';
            format++;
        }
        while (*format >= '0' && *format <= '9')
            pad_width = pad_width * 10 + (*format++ - '0');

        // length modifier: 'l' (long) or 'll' (long long); both 64-bit on LP64
        int long_count = 0;
        while (*format == 'l') {
            long_count++;
            format++;
        }

        switch (*format++) {
        case 'd':
        case 'i': {
            int64_t val = long_count ? (int64_t)__builtin_va_arg(args, long long)
                                     : (int64_t)__builtin_va_arg(args, int);
            itoa(val, tmp, 10);
            int len = 0;
            for (char *p = tmp; *p; p++)
                len++;
            for (int i = len; i < pad_width; i++)
                *out++ = pad_char;
            for (char *p = tmp; *p; p++)
                *out++ = *p;
            break;
        }
        case 'u': {
            uint64_t val = long_count ? (uint64_t)__builtin_va_arg(args, unsigned long long)
                                      : (uint64_t)__builtin_va_arg(args, unsigned int);
            utoa(val, tmp, 10);
            int len = 0;
            for (char *p = tmp; *p; p++)
                len++;
            for (int i = len; i < pad_width; i++)
                *out++ = pad_char;
            for (char *p = tmp; *p; p++)
                *out++ = *p;
            break;
        }
        case 'X':
        case 'x': {
            uint64_t val = long_count ? (uint64_t)__builtin_va_arg(args, unsigned long long)
                                      : (uint64_t)__builtin_va_arg(args, unsigned int);
            utoa(val, tmp, 16);
            int len = 0;
            for (char *p = tmp; *p; p++)
                len++;
            for (int i = len; i < pad_width; i++)
                *out++ = pad_char;
            for (char *p = tmp; *p; p++)
                *out++ = (*(format - 1) == 'X') ? toupper(*p) : *p;
            break;
        }
        case 's': {
            char *s = __builtin_va_arg(args, char *);
            int len = 0;
            for (char *p = s; *p; p++)
                len++;
            if (!left_align)
                for (int i = len; i < pad_width; i++)
                    *out++ = ' ';
            while (*s)
                *out++ = *s++;
            if (left_align)
                for (int i = len; i < pad_width; i++)
                    *out++ = ' ';
            break;
        }
        case 'c':
            *out++ = (char)__builtin_va_arg(args, int);
            break;
        case 'p': {
            uint64_t val = (uint64_t)__builtin_va_arg(args, void *);
            utoa(val, tmp, 16);
            *out++ = '0';
            *out++ = 'x';
            for (char *p = tmp; *p; p++)
                *out++ = *p;
            break;
        }
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

int sprintf(char *str, const char *format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);
    int n = vsprintf(str, format, args);
    __builtin_va_end(args);
    return n;
}