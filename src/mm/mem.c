#include <dtb.h>
#include <arch/cpu.h>
#include <drivers/uart.h>
#include "mem.h"

struct memreg mem;

void mem_init()
{
    if (dtb_get_memory_register(&mem) == 0)
    {
        uart_puts("[mem] base: 0x");
        uart_put_uint_hex(mem.base);
        uart_puts(", size: ");
        uart_put_uint(mem.size / (1024 * 1024));
        uart_puts(" MiB");
        uart_puts("\r\n");
    }
    else
    {
        uart_puts("[mem] Memory register not found!");

        hang();
    }
}