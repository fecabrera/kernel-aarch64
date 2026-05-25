#include <dtb.h>
#include <arch/cpu.h>
#include <arch/irq.h>
#include <arch/syscall.h>
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
    scheduler_init();
    irq_enable();

    // Test RTC alarm
    uint32_t timestamp = rtc_get_time();
    rtc_set_alarm(timestamp + 1);

    // set up pid 1
    if (create_process(&proc, DEFAULT_STACK_SIZE) < 0)
    {
        uart_puts("[kernel] create_process() failed\r\n");
        hang();
    }
    process_config(&proc, &kernel_proc);

    // schedule process
    if (scheduler_enqueue(&proc) == NULL)
    {
        uart_puts("[kernel] scheduler_enqueue() failed\r\n");
        hang();
    }

    // force context switch via syscall
    syscall(SYSCALL_YIELD);
}

void kernel_proc()
{
    uart_puts("switched control to kernel proc...\r\n");

    halt();
}
