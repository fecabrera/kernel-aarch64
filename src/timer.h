#ifndef TIMER_H
#define TIMER_H

#include <types.h>

/**
 * Initializes the ARM generic timer. Reads the timer frequency from cntfrq_el0,
 * sets the countdown from the current interval, enables the timer, and registers
 * the IRQ with the GIC using the IRQ number read from the DTB.
 * timer_set_interval() must be called before timer_init().
 */
void timer_init();

/**
 * IRQ handler for the EL1 physical timer. Increments the tick counter,
 * prints a message every second, and reloads the countdown register.
 */
void timer_handler();

/**
 * Sets the timer tick interval in milliseconds.
 * Must be called before timer_init().
 *
 * @param interval: tick interval in milliseconds (e.g. 10 for 10ms)
 */
void timer_set_interval(uint64_t interval);

#endif // TIMER_H