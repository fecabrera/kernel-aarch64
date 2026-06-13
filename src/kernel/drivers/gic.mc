/**
 * Initializes the GIC distributor and CPU interface.
 * Enables the distributor, enables the CPU interface, and sets the priority
 * mask to accept all interrupt priority levels.
 */
@extern fn gic_init();

/**
 * Enables a specific IRQ in the GIC distributor and routes it to CPU 0
 * at mid priority.
 *
 * @param irq: absolute GIC IRQ ID to enable
 */
@extern fn gic_enable_irq(irq: uint32);

/**
 * Triggers a Software Generated Interrupt on the current CPU by writing
 * to GICD_SGIR with GICD_SGIR_TARGET_SELF.
 *
 * @param irq: SGI ID to trigger (0–15)
 */
@extern fn gic_trigger_sgi(irq: uint32);

/**
 * Acknowledges the highest-priority pending interrupt and moves it to active state.
 * Must be called at the start of an IRQ handler before processing.
 * Returns 1023 if no real interrupt is pending (spurious).
 *
 * @return Absolute GIC IRQ ID of the acknowledged interrupt.
 */
@extern fn gic_acknowledge() -> uint32;

/**
 * Signals end of interrupt to the GIC, allowing the next interrupt of equal
 * or lower priority to be delivered. Must be called at the end of every IRQ handler.
 *
 * @param irq: IRQ ID returned by gic_acknowledge()
 */
@extern fn gic_end_of_interrupt(irq: uint32);
