/**
 * Initializes the ARM generic timer. Reads the timer frequency from cntfrq_el0,
 * sets the countdown from the current interval (default: DEFAULT_TIMER_INTERVAL),
 * enables the timer, and registers the IRQ with the GIC using the IRQ number
 * read from the DTB. Optionally call timer_set_interval() before this to
 * override the default interval.
 */
@extern fn timer_init();

/**
 * Returns the system uptime in milliseconds, computed from cntpct_el0 ticks
 * elapsed since timer_init was called.
 *
 * @return system uptime in milliseconds
 */
@extern fn timer_get_uptime() -> uint64;

/**
 * Sets the timer tick interval in milliseconds.
 * Must be called before timer_init().
 *
 * @param interval: tick interval in milliseconds (e.g. 10 for 10ms)
 */
@extern fn timer_set_interval(interval: uint64);
