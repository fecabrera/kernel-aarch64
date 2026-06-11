#include "stdlib.h"

static const char _itoa_digits[] = "0123456789abcdef";

char *itoa(int64_t value, char *str, int base) {
    char *buff = str;
    int neg = 0;

    if (value == 0) {
        *buff++ = '0';
        *buff = '\0';
        return str;
    }

    if (value < 0 && base == 10) {
        neg = 1;
        value = -value;
    }

    while (value != 0) {
        *buff++ = _itoa_digits[value % base];
        value /= base;
    }

    if (neg)
        *buff++ = '-';

    /* digits were written least-significant first; reverse them */
    char *start = str;
    char *end = buff - 1;
    while (start < end) {
        char tmp = *start;
        *start++ = *end;
        *end-- = tmp;
    }

    *buff = '\0';

    return str;
}