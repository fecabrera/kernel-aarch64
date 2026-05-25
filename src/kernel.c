#include "cpu.h"
#include "uart.h"
#include "gic.h"
#include "irq.h"
#include "timer.h"
#include "dtb.h"
#include "rtc.h"
#include "heap.h"
#include "mem.h"

void kernel_main()
{
    dtb_init();
    dtb_dump();

    heap_init();
    heap_dump();

    mem_init();

    gic_init();
    interrupts_init();
    timer_init();
    uart_init();
    rtc_init();

    timer_set_interval(10);
    irq_enable();

    uint32_t timestamp = rtc_get_time();
    uart_puts("Current timestamp: ");
    uart_put_uint(timestamp);
    uart_puts("\r\n");

    rtc_set_alarm(timestamp + 1);

    halt();
}
