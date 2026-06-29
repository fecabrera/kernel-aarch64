import "debug";
import "cpu";
import "syscall";
import "scheduler";
import "set";
import "range";
import "interrupts/gic";

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
const IRQ_SPI_PL031_RTC_ALARM = 34;      // SPI: PL031 RTC alarm

const NUM_IRQS = 256;

@extern let vector_table: byte*;
@static let _irq_table: struct set<uint32, fn (uint32, struct cpu_context*) -> struct cpu_context*>;

/**
 * Dumps the saved register frame (x0–x14, lr, elr, spsr) to the kernel log.
 * Used by sync_handler to diagnose an unhandled exception before hanging.
 *
 * @param ctx: saved cpu_context to print
 */
fn ctx_dump(ctx: struct cpu_context*) {
    printk("\n=== ctx dump ===\n");
    {
        let r = struct range<int32> { end = 15 };
        for i in &r {
            printk("x%i  = %p\n", i, ctx->x[i]);
        }
    }
    printk("lr  = %p\n", ctx->lr);
    printk("elr = %p\n", ctx->elr);
    printk("spsr= 0b");
    {
        // print all 64 bits of spsr, MSB first
        let i = sizeof(uint64) as int32 * 8 - 1;
        until (i < 0) {
            printk("%d", (ctx->spsr as int32 >> i) & 1);
            i = i - 1;
        }
    }
    printk("\n=================\n");
}

/**
 * Looks up the handler registered for the given IRQ ID in the dispatch table.
 *
 * @param irq: GIC IRQ ID to look up
 *
 * @return the registered handler, or null if none is registered
 */
@private
fn get_irq_handler(irq: uint32) -> fn (uint32, struct cpu_context*) -> struct cpu_context* {
    let fnc: fn (uint32, struct cpu_context*) -> struct cpu_context*;
    if (!set_get(&_irq_table, irq, &fnc))
        return null;

    return fnc;
}

/**
 * Stores fnc as the handler for the given IRQ ID, overwriting any existing entry.
 *
 * @param irq: GIC IRQ ID to bind
 * @param fnc: handler to store
 */
@private
fn set_irq_handler(irq: uint32, fnc: fn (uint32, struct cpu_context*) -> struct cpu_context*) {
    set_set(&_irq_table, irq, fnc);
}

/**
 * Removes the dispatch-table entry for the given IRQ ID, if any.
 *
 * @param irq: GIC IRQ ID to clear
 */
@private
fn remove_irq_handler(irq: uint32) {
    set_remove(&_irq_table, irq);
}

/**
 * Installs the exception vector table by writing its address to vbar_el1,
 * followed by an ISB to ensure the CPU sees the new table immediately.
 * Also initializes the internal set64-backed IRQ dispatch table.
 * Must be called before irq_register_handler or irq_handler.
 */
fn irq_init() {
    // Point CPU to our vector table
    set_vbar_el1(&vector_table as uint64);

    // initialize irq table
    set_init(&_irq_table, 10);
}

/**
 * Registers a handler for the given GIC IRQ ID.
 *
 * @param irq: GIC IRQ ID (0–255)
 * @param callback: function to call when the interrupt fires
 *
 * @return 0 on success, -1 if a handler is already registered for that IRQ
 */
fn irq_register_handler(irq: uint32,
                        callback: fn (uint32, struct cpu_context*) -> struct cpu_context*) -> int32 {
    if (get_irq_handler(irq) == null) {
        set_irq_handler(irq, callback);
        dprintk("[irq] handler registered for IRQ %i, address = %p\n", irq, callback);
        return 0;
    }
    
    dprintk("[irq] There's already a handler registered for IRQ %i!\n", irq);
    return -1;
}

/**
 * Removes the handler for the given GIC IRQ ID.
 * After this call, unhandled interrupts for that ID will log a warning.
 *
 * @param irq: GIC IRQ ID (0–255)
 *
 * @return 0 on success, -1 if no handler was registered for that IRQ
 */
fn irq_unregister_handler(irq: uint32) -> int32 {
    if (get_irq_handler(irq) == null) {
        dprintk("[irq] There's no handler registered for IRQ %i!\n", irq);
        return -1;
    }
    
    remove_irq_handler(irq);
    dprintk("[irq] Handler unregistered for IRQ %i!\n", irq);
    return 0;
}

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
fn sync_handler(ctx: struct cpu_context*, esr: uint64, elr: uint64, far: uint64) -> struct cpu_context* {
    dprintk("[sync] sync_handler(), ctx=%p, esr=%p, elr=%p, far=%p\n", ctx, esr, elr, far);
    
    let ec: uint32 = ((esr >> ESR_EC_SHIFT) & ESR_EC_MASK) as uint32;
    
    case (ec) {
    when ESR_EC_SVC64:
        dprintk("[sync] syscall(%d)\n", ctx->x[0]);
        ctx = syscall_handler(ctx);
    when ESR_EC_IABT_EL0:
        dprintk("[sync] instruction abort, elr = %p, far %p\n", elr, far);
        ctx = syscall_exit_handler(ctx);
    when ESR_EC_DABT_EL0:
        dprintk("[sync] data abort, elr = %p, far = %p\n", elr, far);
        ctx = syscall_exit_handler(ctx);
    else:
        printk("[sync] esr = %p, elr = %p, far = %p, ec = %02x\n", esr, elr, far, ec);
        ctx_dump(ctx);

        hang();
    }

    return ctx;
}

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
fn irq_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    // Ask the GIC which interrupt fired (see below)
    let irq_id = gic_acknowledge();
    let fnc = get_irq_handler(irq_id);

    if (fnc == null)
        dprintk("[irq] Handler not found for IRQ %i!\n", irq_id);
    else
        ctx = fnc(irq_id, ctx);

    gic_end_of_interrupt(irq_id);

    return ctx;
}

/**
 * Handles FIQ exceptions. Currently logs and returns.
 */
fn fiq_handler() {
    dprintk("[FIQ]\n");
}

/**
 * Handles SError (System Error) exceptions. Logs and halts — SErrors are
 * typically unrecoverable hardware faults.
 */
fn serr_handler() {
    dprintk("[SError] System error!\n");
    hang();
}
