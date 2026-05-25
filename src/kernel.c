#include "cpu.h"
#include "uart.h"
#include "gic.h"
#include "irq.h"
#include "timer.h"
#include "dtb.h"
#include "rtc.h"
#include "heap.h"

extern void *dtb_ptr;

void kernel_main()
{
    dtb_init(dtb_ptr);
    dtb_dump();

    struct memreg mem;
    if (dtb_get_memory_register(&mem) == 0)
    {
        uart_puts("Memory base: 0x");
        uart_put_uint_base(mem.base, 16);
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

    gic_init();
    interrupts_init();
    timer_init();
    uart_init();
    rtc_init();

    timer_set_interval(10);
    irq_enable();

    heap_init();

    uint32_t timestamp = rtc_get_time();
    uart_puts("Current timestamp: ");
    uart_put_uint(timestamp);
    uart_puts("\r\n");

    rtc_set_alarm(timestamp + 1);

    halt();
}
