#include <dtb.h>
#include <arch/cpu.h>
#include <arch/irq.h>
#include <drivers/uart.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <drivers/rtc.h>
#include <mm/heap.h>
#include <mm/mem.h>
#include <sched/process.h>
#include <sched/scheduler.h>
#include "kernel.h"

static struct process proc;

void kernel_init()
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
    irq_init();
    timer_init();
    uart_init();
    rtc_init();
    irq_enable();

    // Test RTC alarm
    uint32_t timestamp = rtc_get_time();
    rtc_set_alarm(timestamp + 1);

    // set up pid 1
    if (create_process(&proc, DEFAULT_STACK_SIZE) < 0)
    {
        uart_puts("[kernel] out of memory\r\n");
        hang();
    }
    process_config(&proc, &kernel_proc);

    // schedule process
    scheduler_enqueue(&proc);

    // just one timer interrupt is needed for control to be effectively
    // handed over to kernel_proc, though other interrupts could be
    // triggered before
    halt();
}

void kernel_proc()
{
    uart_puts("switched control to kernel proc...\r\n");

    halt();
}
