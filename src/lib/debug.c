#include <drivers/uart.h>
#include "debug.h"

void printk(const char *format, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, format);
    uart_vprintf(format, args);
    __builtin_va_end(args);
}

void dprintk(const char *format, ...)
{
#ifdef DEBUG
    __builtin_va_list args;
    __builtin_va_start(args, format);
    uart_vprintf(format, args);
    __builtin_va_end(args);
#endif
}
