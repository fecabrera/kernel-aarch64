import "debug";
import "cpu";
import "dtb";
import "syscall";
import "interrupts/gic";
import "interrupts/irq";

/**
 * PL031 RTC register block, memory-mapped at RTC_BASE. All registers are 32-bit
 * and laid out contiguously from offset 0x000; access them through the PL031
 * pointer (e.g. PL031->dr).
 */
@volatile
struct pl031_regs {
    dr: uint32;   // 0x000 Data (current time, read-only)
    mr: uint32;   // 0x004 Match (alarm time)
    lr: uint32;   // 0x008 Load (set current time)
    cr: uint32;   // 0x00C Control
    imsc: uint32; // 0x010 Interrupt mask
    ris: uint32;  // 0x014 Raw interrupt status
    mis: uint32;  // 0x018 Masked interrupt status
    icr: uint32;  // 0x01C Interrupt clear
}

const RTC_BASE = 0x09010000;

// CR flags
const RTC_CR_EN = (1 << 0); // RTC enable

// Interrupt bits (same bit position in IMSC, RIS, MIS, ICR)
const RTC_INT_MATCH = (1 << 0); // Match interrupt

// Memory-mapped PL031 register block
@static let PL031: struct pl031_regs* = RTC_BASE as struct pl031_regs*;

@static let pl031_irq: uint32;

/**
 * Enables the RTC by setting the EN bit in CR, reads the IRQ number from the
 * DTB, registers pl031_irq_handler with the GIC, and registers syscall_time_handler for
 * SYSCALL_TIME.
 */
fn pl031_init() {
    PL031->cr = RTC_CR_EN;

    if (dtb_get_rtc_irq_number(&pl031_irq) == 0) {
        dprintk("[pl031] Initializing IRQ: %i\n", pl031_irq);
        irq_register_handler(pl031_irq, pl031_irq_handler);
        syscall_register_handler(SYSCALL_TIME, syscall_time_handler);
    } else {
        dprintk("[pl031] IRQ not found!!\n");
    }
}

/**
 * Reads the current time from the RTC data register.
 *
 * @return Current Unix timestamp.
 */
fn pl031_get_time() -> uint32 {
    return PL031->dr;
}

/**
 * Sets the current time by writing to the RTC load register.
 *
 * @param unix_time: Unix timestamp to set
 */
fn pl031_set_time(unix_time: uint32) {
    PL031->lr = unix_time;
}

/**
 * Sets an alarm to fire at the given Unix timestamp.
 * Writes to the match register, unmasks the match interrupt, and enables the GIC IRQ.
 *
 * @param unix_time: Unix timestamp at which the alarm should fire
 */
fn pl031_set_alarm(unix_time: uint32) {
    PL031->mr = unix_time;
    PL031->imsc = RTC_INT_MATCH;

    gic_enable_irq(pl031_irq);
}

/**
 * IRQ handler for the RTC match interrupt. Clears the interrupt flag and
 * re-masks the interrupt until the next alarm is set. Registered with
 * irq_register_handler at pl031_init time.
 *
 * @param irq: IRQ ID passed by the dispatcher (unused)
 * @param ctx: saved register frame from the interrupted context
 *
 * @return ctx unchanged (no context switch).
 */
fn pl031_irq_handler(irq: uint32, ctx: struct cpu_context*) -> struct cpu_context* {
    dprintk("[pl031] alarm fired!\n");

    PL031->icr = RTC_INT_MATCH;   // clear interrupt
    PL031->imsc = ~RTC_INT_MATCH as uint32; // mask

    return ctx;
}

/**
 * Syscall handler for SYSCALL_TIME. Reads the current Unix timestamp from
 * the RTC data register (PL031->dr) and writes it into ctx->x0. Does not
 * perform a context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx unchanged, with ctx->x0 set to the current Unix timestamp
 */
fn syscall_time_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    dprintk("[pl031] time()\n");

    ctx->x[0] = pl031_get_time() as uint64;
    return ctx;
}