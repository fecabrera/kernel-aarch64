/**
 * Executes the WFE (Wait For Event) instruction, suspending the CPU until an
 * event or interrupt is signalled. Less aggressive than WFI — wakes on SEV
 * (send event) from another core as well as interrupts.
 */
@extern fn wfe();

/**
 * Executes the WFI (Wait For Interrupt) instruction, suspending the CPU until
 * an interrupt is pending. Used in spin loops to avoid busy-waiting.
 */
@extern fn wfi();

/**
 * Clears the DAIF I-bit, unmasking IRQ exceptions at the current EL.
 */
@extern fn irq_enable();

/**
 * Sets the DAIF I-bit, masking IRQ exceptions at the current EL.
 */
@extern fn irq_disable();

/**
 * Halts the CPU in a low-power loop using wfi, waking only to service
 * interrupts before returning to sleep.
 */
@extern fn halt();

/**
 * Spins forever with interrupts disabled. Used for unrecoverable errors
 * where the system must stop completely.
 */
@extern fn hang();

/**
 * Reads the physical count register (cntpct_el0).
 * Increments at the frequency reported by cntfrq_el0 (typically 24 MHz on
 * QEMU virt). Divide by get_cntfrq_el0() to convert ticks to seconds.
 *
 * @return Current 64-bit physical counter value.
 */
@extern fn get_cntpct_el0() -> uint64;

/**
 * Reads the timer frequency register (cntfrq_el0).
 *
 * @return Timer frequency in Hz as set by firmware.
 */
@extern fn get_cntfrq_el0() -> uint64;

/**
 * Writes the EL1 physical timer value register (cntp_tval_el0).
 * Starts a countdown from value; the timer fires IRQ when it reaches zero.
 * Must be rewritten each tick to reload the countdown.
 *
 * @param value: number of timer ticks until the next interrupt
 */
@extern fn set_cntp_tval_el0(value: uint64);