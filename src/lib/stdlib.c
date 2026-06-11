#include "stdlib.h"
#include "stdbool.h"

static const char _itoa_digits[] = "0123456789abcdef";

char *itoa(int64_t value, char *str, int base) {
    char *buff = str;
    bool neg = false;

    if (value == 0) {
        *buff++ = '0';
        *buff = '\0';
        return str;
    }

    if (value < 0 && base == 10) {
        neg = true;
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

static long long _ato(const char *str) {
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r' || *str == '\f' ||
           *str == '\v')
        str++;

    int sign = 1;
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }

    long long result = 0;
    while (*str >= '0' && *str <= '9')
        result = result * 10 + (*str++ - '0');

    return sign * result;
}

int atoi(const char *str) { return (int)_ato(str); }
long atol(const char *str) { return (long)_ato(str); }
long long atoll(const char *str) { return _ato(str); }