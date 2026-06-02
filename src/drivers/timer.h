#pragma once

#include <stdint.h>
#include <time.h>

#define DEFAULT_TIMER_INTERVAL 10

/**
 * Timing state maintained by the timer driver across each tick interrupt.
 * Used to compute the real elapsed interval and system uptime in milliseconds.
 */
struct sched_info
{
    uint64_t ticks;         // cntpct_el0 value captured at the last tick interrupt
    uint64_t initial_ticks; // cntpct_el0 value captured at timer_init time
    uint64_t frequency;     // counter frequency in Hz, read from cntfrq_el0
    time_t interval;        // desired tick interval in milliseconds
};

/**
 * Initializes the ARM generic timer. Reads the timer frequency from cntfrq_el0,
 * sets the countdown from the current interval (default: DEFAULT_TIMER_INTERVAL),
 * enables the timer, and registers the IRQ with the GIC using the IRQ number
 * read from the DTB. Optionally call timer_set_interval() before this to
 * override the default interval.
 */
void timer_init();

/**
 * Returns the system uptime in milliseconds, computed from cntpct_el0 ticks
 * elapsed since timer_init was called.
 *
 * @return system uptime in milliseconds
 */
time_t timer_get_uptime();

/**
 * IRQ handler for the EL1 physical timer. Increments the tick counter,
 * prints a message every second, reloads the countdown register, and
 * calls scheduler_handler to preempt the current task.
 * Registered with irq_register_handler at timer_init time.
 *
 * @param irq: IRQ ID passed by the dispatcher (unused)
 * @param ctx: saved register frame from the interrupted context
 *
 * @return saved context of the next task to run as selected by the scheduler
 */
struct cpu_context *timer_irq_handler(int irq, struct cpu_context *ctx);

/**
 * Syscall handler for SYSCALL_UPTIME. Reads the current cntpct_el0 tick count,
 * computes elapsed milliseconds since timer_init, and returns the result in x0.
 *
 * @param ctx: saved register frame of the calling process
 *
 * @return ctx with x0 set to the system uptime in milliseconds
 */
struct cpu_context *syscall_uptime_handler(struct cpu_context *ctx);

/**
 * Sets the timer tick interval in milliseconds.
 * Must be called before timer_init().
 *
 * @param interval: tick interval in milliseconds (e.g. 10 for 10ms)
 */
void timer_set_interval(time_t interval);
