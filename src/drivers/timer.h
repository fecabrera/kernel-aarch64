#pragma once

/**
 * Initializes the ARM generic timer. Reads the timer frequency from cntfrq_el0,
 * sets the countdown from the current interval (default: DEFAULT_TIMER_INTERVAL),
 * enables the timer, and registers the IRQ with the GIC using the IRQ number
 * read from the DTB. Optionally call timer_set_interval() before this to
 * override the default interval.
 */
void timer_init();