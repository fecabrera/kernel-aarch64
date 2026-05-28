#include <dtb.h>
#include <arch/cpu.h>
#include <drivers/uart.h>
#include "mem.h"

struct memreg mem;

void mem_init()
{
    if (dtb_get_memory_register(&mem) == 0)
    {
        uart_printf("[mem] base: 0x%x, size=%i MiB\r\n", mem.base, mem.size / (1024 * 1024));
    }
    else
    {
        uart_printf("[mem] Memory register not found!");

        hang();
    }
}