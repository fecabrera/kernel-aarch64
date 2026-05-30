#include <drivers/pl011.h>
#include "debug.h"

void printk(const char *format, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, format);
    pl011_vprintf(format, args);
    __builtin_va_end(args);
}

#ifdef DEBUG
void dprintk(const char *format, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, format);
    pl011_vprintf(format, args);
    __builtin_va_end(args);
}
#else
void dprintk(__attribute__((unused)) const char *format, ...)
{
}
#endif
