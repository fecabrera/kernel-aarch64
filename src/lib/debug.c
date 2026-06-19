#include "debug.h"
#include <drivers/pl011.h>
#include <stdarg.h>

// #ifdef DEBUG
// void dprintk(const char *format, ...) {
//     va_list args;
//     va_start(args, format);
//     pl011_vprintf(format, args);
//     va_end(args);
// }
// #else
// void dprintk(__attribute__((unused)) const char *format, ...) {}
// #endif
