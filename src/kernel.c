#include <dtb.h>
#include <debug.h>
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

static void _kernel_init()
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
}

void kernel_init()
{
    _kernel_init();

    // test RTC alarm
    uint32_t timestamp = rtc_get_time();
    rtc_set_alarm(timestamp + 1);

    // test yield
    printk("[kernel] testing yield()\r\n");
    int64_t result = syscall_yield();

    // since there are no other processes, there's no context switch and we should
    // be back immediately with the same context
    printk("[kernel] back from yield(), result = %i\r\n", result);

    // set up pid 1
    pid_t pid = scheduler_spawn(&init);
    printk("[kernel] spawned init process with pid = %i\r\n", pid);

    // force context switch via syscall
    result = syscall_yield();

    // note that since there are items in the ready queue, this becomes unreachable
    // @todo: add idle task to run when ready queue is empty, then this becomes reachable
    printk("[kernel] unexpectedly back from yield(), result = %i\r\n", result);
}

void init()
{
    pid_t pid = syscall_getpid();

    printk("[init] pid = %i\r\n", pid);

    pid_t fork_pid = syscall_fork();

    if (fork_pid < 0)
    {
        printk("[child] fork() failed!\r\n");
        halt();
    }

    if (fork_pid == 0)
        return child();

    int64_t exit_status = syscall_waitpid(fork_pid);
    printk("[init] child process (pid %i) terminated with status %i\r\n", fork_pid, exit_status);

    halt();
}

void child()
{
    pid_t child_pid = syscall_fork();

    if (child_pid < 0)
    {
        printk("[child] fork() failed!\r\n");
        syscall_exit(2);
    }

    pid_t pid = syscall_getpid();
    printk("[child] pid = %i, child_pid = %i\r\n", pid, child_pid);

    if (child_pid == 0)
    {
        printk("[child] going to sleep\r\n");

        time_t t0 = syscall_time();
        syscall_sleep(2);
        time_t t1 = syscall_time();

        printk("[child] back from sleep, elapsed = %is\r\n", t1 - t0);
        syscall_exit(0);
    }
    else
    {
        syscall_exit(1);
    }
}
