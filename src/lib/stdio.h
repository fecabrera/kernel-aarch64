#pragma once

#include "stdarg.h"

#ifndef STB_SPRINTF_DECORATE
#define STB_SPRINTF_DECORATE(name) name
#endif

#ifndef STB_SPRINTF_NOFLOAT
#define STB_SPRINTF_NOFLOAT
#endif

#ifndef STB_SPRINTF_NOUNALIGNED
#define STB_SPRINTF_NOUNALIGNED
#endif

#include "stb_sprintf.h"

int printf(const char *format, ...);
int vprintf(const char *format, va_list args);