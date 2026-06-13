/**
 * Enables the RTC by setting the EN bit in CR, reads the IRQ number from the
 * DTB, registers pl031_irq_handler with the GIC, and registers time_handler for
 * SYSCALL_TIME.
 */
@extern fn pl031_init();

/**
 * Reads the current time from the RTC data register.
 *
 * @return Current Unix timestamp.
 */
@extern fn pl031_get_time();

/**
 * Sets the current time by writing to the RTC load register.
 *
 * @param unix_time: Unix timestamp to set
 */
@extern fn pl031_set_time(unix_time: uint32);

/**
 * Sets an alarm to fire at the given Unix timestamp.
 * Writes to the match register, unmasks the match interrupt, and enables the GIC IRQ.
 *
 * @param unix_time: Unix timestamp at which the alarm should fire
 */
@extern fn pl031_set_alarm(unix_time: uint32);