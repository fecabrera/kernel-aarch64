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
    uart_printf("[kernel] testing yield()\r\n");
    int64_t result = syscall_yield();

    // since there are no other processes, there's no context switch and we should
    // be back immediately with the same context
    uart_printf("[kernel] back from yield(), result = %i\r\n", result);

    // set up pid 1
    struct process proc;

    if (create_process(&proc, DEFAULT_STACK_SIZE) < 0)
    {
        uart_printf("[kernel] create_process() failed\r\n");
        hang();
    }
    process_config(&proc, &init);

    // schedule process
    if (scheduler_enqueue(&proc) < 0)
    {
        uart_printf("[kernel] scheduler_enqueue() failed\r\n");
        hang();
    }

    // force context switch via syscall
    result = syscall_yield();

    // note that since there are items in the ready queue, this becomes unreachable
    // @todo: add idle task to run when ready queue is empty, then this becomes reachable
    uart_printf("[kernel] unexpectedly back from yield(), result = %i\r\n", result);
}

void init()
{
    int64_t pid = syscall_getpid();

    uart_printf("[init] pid = %i\r\n", pid);

    struct process proc;

    if (create_process(&proc, DEFAULT_STACK_SIZE) < 0)
    {
        uart_printf("[init] create_process() failed\r\n");
        hang();
    }
    process_config(&proc, &child);

    if (scheduler_enqueue(&proc) < 0)
    {
        uart_printf("[init] scheduler_enqueue() failed\r\n");
        hang();
    }

    uint64_t exit_status = syscall_waitpid(proc.pid);
    uart_printf("[init] child process terminated with status %i\r\n", exit_status);

    halt();
}

void child()
{
    int64_t fork_pid = syscall_fork();
    int64_t pid = syscall_getpid();

    uart_printf("[child] pid = %i, fork_pid = %i\r\n", pid, fork_pid);

    if (fork_pid < 0)
    {
        uart_printf("[child] fork() failed!\r\n");
        syscall_exit(2);
    }
    else if (fork_pid == 0)
        syscall_exit(0);
    else
        syscall_exit(1);
}
