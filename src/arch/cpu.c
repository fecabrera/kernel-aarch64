#include "cpu.h"
#include <stdbool.h>

void halt() {
    _wfi_while(true);
    __builtin_unreachable();
}

void hang() {
    _wfe_while(true);
    __builtin_unreachable();
}