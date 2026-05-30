#include <dtb.h>
#include <arch/cpu.h>
#include <debug.h>
#include "mem.h"

static struct memreg mem;

void mem_init()
{
    if (dtb_get_memory_register(&mem) == 0)
        dprintk("[mem] base: 0x%x, size=%i MiB\r\n", mem.base, mem.size / (1 << 20));
    else
        dprintk("[mem] Memory register not found!");
}