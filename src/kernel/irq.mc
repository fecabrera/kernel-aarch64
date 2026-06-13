import "cpu.h";

@extern let vector_table: uint8*;

/**
 * Installs the exception vector table by writing its address to vbar_el1,
 * followed by an ISB to ensure the CPU sees the new table immediately.
 * Also initializes the internal set64-backed IRQ dispatch table.
 * Must be called before irq_register_handler or irq_handler.
 */
@extern fn irq_init();

/**
 * Registers a handler for the given GIC IRQ ID.
 *
 * @param irq: GIC IRQ ID (0–255)
 * @param callback: function to call when the interrupt fires
 *
 * @return 0 on success, -1 if a handler is already registered for that IRQ
 */
@extern fn irq_register_handler(irq: uint32, callback:
                                fn (int32, struct cpu_context *) -> struct cpu_context*) -> int32;

/**
 * Removes the handler for the given GIC IRQ ID.
 * After this call, unhandled interrupts for that ID will log a warning.
 *
 * @param irq: GIC IRQ ID (0–255)
 *
 * @return 0 on success, -1 if no handler was registered for that IRQ
 */
@extern fn irq_unregister_handler(irq: uint32) -> int32;

/**
 * Handles synchronous exceptions (data aborts, instruction aborts, syscalls).
 * Decodes the exception class from ESR_EL1 and dispatches accordingly.
 * For syscalls (ESR_EC_SVC64), may return a different context to perform
 * a context switch. For faults, hangs or returns ctx unchanged.
 *
 * @param ctx: pointer to the saved cpu_context on the current task's stack
 * @param esr: Exception Syndrome Register value at the time of the exception
 * @param elr: Exception Link Register — address of the faulting instruction
 * @param far: Fault Address Register — virtual address that caused the fault
 *
 * @return pointer to the saved cpu_context of the next task to run
 */
@extern fn sync_handler(ctx: struct cpu_context*, esr: uint64, elr: uint64, far: uint64) -> struct cpu_context*;

/**
 * Handles IRQ exceptions. Acknowledges the interrupt from the GIC,
 * dispatches to the registered handler, then signals end of interrupt.
 * Called from the IRQ vector stub in vectors.S with the saved register frame.
 *
 * @param ctx: pointer to the saved cpu_context on the current task's stack
 *
 * @return pointer to the saved cpu_context of the next task to run;
 *         the assembly stub writes this into sp before restore_context and eret.
 */
@extern fn irq_handler(ctx: struct cpu_context*) -> struct cpu_context*;

/**
 * Handles FIQ exceptions. Currently logs and returns.
 */
@extern fn fiq_handler();

/**
 * Handles SError (System Error) exceptions. Logs and halts — SErrors are
 * typically unrecoverable hardware faults.
 */
@extern fn serr_handler();
