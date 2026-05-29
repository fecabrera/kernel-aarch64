#pragma once

#include <stdint.h>
#include <time.h>

#define DEFAULT_TIMER_INTERVAL 10

/**
 * Initializes the ARM generic timer. Reads the timer frequency from cntfrq_el0,
 * sets the countdown from the current interval (default: DEFAULT_TIMER_INTERVAL),
 * enables the timer, and registers the IRQ with the GIC using the IRQ number
 * read from the DTB. Optionally call timer_set_interval() before this to
 * override the default interval.
 */
void timer_init();

/**
 * IRQ handler for the EL1 physical timer. Increments the tick counter,
 * prints a message every second, reloads the countdown register, and
 * calls scheduler_handler to preempt the current task.
 * Registered with irq_register_handler at timer_init time.
 *
 * @param ctx: saved register frame from the interrupted context
 *
 * @return saved context of the next task to run as selected by the scheduler
 */
struct cpu_context *timer_irq_handler(struct cpu_context *ctx);

/**
 * Sets the timer tick interval in milliseconds.
 * Must be called before timer_init().
 *
 * @param interval: tick interval in milliseconds (e.g. 10 for 10ms)
 */
void timer_set_interval(time_t interval);
