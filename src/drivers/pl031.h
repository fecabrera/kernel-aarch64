#pragma once

/**
 * Enables the RTC by setting the EN bit in CR, reads the IRQ number from the
 * DTB, registers pl031_irq_handler with the GIC, and registers syscall_time_handler for
 * SYSCALL_TIME.
 */
void pl031_init();