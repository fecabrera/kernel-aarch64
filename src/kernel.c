#include <dtb.h>
#include <arch/cpu.h>
#include <arch/irq.h>
#include <drivers/uart.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <drivers/rtc.h>
#include <mm/heap.h>
#include <mm/mem.h>

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
