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
    // Initialize DTB
    dtb_init();
    dtb_dump();

    // Initialize memory
    mem_init();
    heap_init();
    heap_dump();

    // Initialize interrupts
    gic_init();
    interrupts_init();
    timer_init();
    uart_init();
    rtc_init();
    irq_enable();

    // Test RTC alarm
    uint32_t timestamp = rtc_get_time();
    rtc_set_alarm(timestamp + 1);

    // Halt
    halt();
}
