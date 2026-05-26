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

static struct process proc1, proc2;

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

    // test RTC alarm
    uint32_t timestamp = rtc_get_time();
    rtc_set_alarm(timestamp + 1);

    // test yield
    uart_puts("[kernel] testing yield()\r\n");
    int64_t result = syscall_yield();
    uart_puts("[kernel] back from yield(), result = ");
    uart_put_uint(result);
    uart_puts("\r\n");

    // set up pid 1
    if (create_process(&proc1, DEFAULT_STACK_SIZE) < 0)
    {
        uart_puts("[kernel] create_process() failed\r\n");
        hang();
    }
    process_config(&proc1, &init);

    // schedule process
    if (scheduler_enqueue(&proc1) == NULL)
    {
        uart_puts("[kernel] scheduler_enqueue() failed\r\n");
        hang();
    }

    // force context switch via syscall
    result = syscall_yield();
    uart_puts("[kernel] unexpectedly back from yield(), result = ");
    uart_put_uint(result);
    uart_puts("\r\n");
}

void init()
{
    uart_puts("[init] taking control...\r\n");

    if (create_process(&proc2, DEFAULT_STACK_SIZE) < 0)
    {
        uart_puts("[init] create_process() failed\r\n");
        hang();
    }
    process_config(&proc2, &child);

    if (scheduler_enqueue(&proc2) == NULL)
    {
        uart_puts("[init] scheduler_enqueue() failed\r\n");
        hang();
    }

    halt();
}

void child()
{
    uart_puts("[child] Hello from the child process!\r\n");

    halt();
}
