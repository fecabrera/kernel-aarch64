#include "pl011.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void pl011_vprintf(const char *format, __builtin_va_list args) {
    char tmp[32];

    while (*format) {
        if (*format != '%') {
            pl011_putc(*format++);
            continue;
        }

        format++;

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

        switch (*format++) {
        case 'd':
        case 'i': {
            int val = __builtin_va_arg(args, int);
            itoa((int64_t)val, tmp, 10);
            int len = strlen(tmp);
            for (int i = len; i < pad_width; i++)
                pl011_putc(pad_char);
            for (char *p = tmp; *p; p++)
                pl011_putc(*p);
            break;
        }
        case 'u': {
            unsigned int val = __builtin_va_arg(args, unsigned int);
            itoa((int64_t)val, tmp, 10);
            int len = strlen(tmp);
            for (int i = len; i < pad_width; i++)
                pl011_putc(pad_char);
            for (char *p = tmp; *p; p++)
                pl011_putc(*p);
            break;
        }
        case 'X':
        case 'x': {
            unsigned int val = __builtin_va_arg(args, unsigned int);
            itoa((int64_t)val, tmp, 16);
            int len = strlen(tmp);
            for (int i = len; i < pad_width; i++)
                pl011_putc(pad_char);
            for (char *p = tmp; *p; p++)
                pl011_putc((*(format - 1) == 'X') ? toupper(*p) : *p);
            break;
        }
        case 's': {
            char *s = __builtin_va_arg(args, char *);
            int len = strlen(s);
            if (!left_align)
                for (int i = len; i < pad_width; i++)
                    pl011_putc(' ');
            while (*s)
                pl011_putc(*s++);
            if (left_align)
                for (int i = len; i < pad_width; i++)
                    pl011_putc(' ');
            break;
        }
        case 'c':
            pl011_putc((char)__builtin_va_arg(args, int));
            break;
        case '%':
            pl011_putc('%');
            break;
        default:
            pl011_putc('%');
            pl011_putc(*(format - 1));
            break;
        }
    }
}
