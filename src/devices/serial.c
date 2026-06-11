#include "serial.h"
#include <arch/cpu.h>
#include <debug.h>
#include <drivers/pl011.h>
#include <io/module.h>

void serial_init() {
    char *mod_name = "serial";
    dprintk("[storage] adding \"/dev/%s\"\r\n", mod_name);
    io_register_module(mod_name, 0, &serial_read, &serial_write);
}

int serial_read(uint8_t *buffer, size_t count, __attribute__((unused)) size_t offset,
                __attribute__((unused)) uint64_t drv_info) {
    char c;
    for (size_t i = 0; i < count; i++) {
        while (pl011_getc(&c))
            wfi();
        *buffer++ = c;
    }
    return count;
}

int serial_write(uint8_t *buffer, size_t count, __attribute__((unused)) size_t offset,
                 __attribute__((unused)) uint64_t drv_info) {
    for (size_t i = 0; i < count; i++)
        pl011_putc(buffer[i]);
    return count;
}