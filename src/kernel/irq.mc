import "cpu.h";

const ESR_EC_SHIFT = 26;
const ESR_EC_MASK = 0x3F;

// EC values (ARMv8-A Architecture Reference Manual, Table D17-2)
const ESR_EC_UNKNOWN = 0x00;   // Unknown reason
const ESR_EC_WFx = 0x01;       // Trapped WFI or WFE instruction
const ESR_EC_FP_ACCESS = 0x07; // Trapped access to SVE/SIMD/FP
const ESR_EC_ILL_STATE = 0x0E; // SP alignment fault (SP not 16-byte aligned)
const ESR_EC_FP_EXC = 0x2C;    // Trapped floating-point exception
const ESR_EC_SVC64 = 0x15;     // SVC executed in AArch64 (syscall entry)
const ESR_EC_IABT_EL0 = 0x20;  // Instruction abort from EL0 (page fault)
const ESR_EC_IABT_EL1 = 0x21;  // Instruction abort from EL1
const ESR_EC_PC_ALIGN = 0x22;  // PC alignment fault (PC not 4-byte aligned)
const ESR_EC_DABT_EL0 = 0x24;  // Data abort from EL0 (page fault / bus error)
const ESR_EC_DABT_EL1 = 0x25;  // Data abort from EL1
const ESR_EC_SERROR = 0x2F;    // SError interrupt (async system error)

const IRQ_PPI_EL2_HYPERVISOR_TIMER = 27; // PPI: EL2 hypervisor physical timer
const IRQ_PPI_EL1_VIRTUAL_TIMER = 28;    // PPI: EL1 virtual timer
const IRQ_PPI_EL3_SECURE_TIMER = 29;     // PPI: EL3 secure physical timer
const IRQ_PPI_EL1_PHYSICAL_TIMER = 30;   // PPI: EL1 physical timer
const IRQ_SPI_PL011_UART0 = 32;          // SPI: PL011 UART0
const IRQ_SPI_PL011_UART1 = 33;          // SPI: PL011 UART1
const IRQ_SPI_PL031_RTC_ALARM = 34;      // SPI: PL011 RTC alarm

const NUM_IRQS = 256;

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
