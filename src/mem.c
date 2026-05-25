#include "mem.h"
#include "dtb.h"
#include "uart.h"

void mem_init()
{
    struct memreg mem;
    if (dtb_get_memory_register(&mem) == 0)
    {
        uart_puts("Memory base: 0x");
        uart_put_uint_hex(mem.base);
        uart_puts("\r\n");

        uart_puts("Memory size: ");
        uart_put_uint(mem.size / (1024 * 1024));
        uart_puts(" MiB");
        uart_puts("\r\n");
    }
    else
    {
        uart_puts("Memory register not found!");
    }
}