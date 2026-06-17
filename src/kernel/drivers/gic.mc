/**
 * GIC distributor register block, memory-mapped at GICD_BASE. The named
 * registers sit at fixed offsets with reserved gaps between them; the banked
 * registers (isenabler, ipriorityr, itargetsr, icfgr) are word arrays indexed
 * by IRQ ID. Access through the GICD pointer (e.g. GICD->ctlr).
 */
@volatile
struct gicd_regs {
    ctlr: uint32;                   // 0x000 Distributor control
    _reserved0: uint32[0x3F];       // 0x004–0x0FC
    isenabler: uint32[32];          // 0x100 Set-enable (1 bit/IRQ)
    _reserved1: uint32[0xA0];       // 0x180–0x3FC
    ipriorityr: uint32[256];        // 0x400 Priority (1 byte/IRQ)
    itargetsr: uint32[256];         // 0x800 CPU targets (1 byte/IRQ)
    icfgr: uint32[64];              // 0xC00 Config (2 bits/IRQ)
    _reserved2: uint32[0x80];       // 0xD00–0xEFC
    sgir: uint32;                   // 0xF00 Software-generated interrupt
}

/**
 * GIC CPU interface register block, memory-mapped at GICC_BASE. Registers are
 * 32-bit; a one-word gap at 0x008 (the unused BPR) separates pmr from iar.
 * Access through the GICC pointer (e.g. GICC->iar).
 */
@volatile
struct gicc_regs {
    ctlr: uint32; // 0x000 CPU interface control
    pmr: uint32;  // 0x004 Interrupt priority mask
    _reserved0: uint32;    // 0x008 (BPR, unused)
    iar: uint32;  // 0x00C Interrupt acknowledge
    eoir: uint32; // 0x010 End of interrupt
}

const GICD_BASE = 0x08000000;
const GICC_BASE = 0x08010000;

// GICD_SGIR — target list filter (bits 25:24)
const GICD_SGIR_TARGET_LIST = (0 << 24);   // send to CPUs in target list (bits 19:16)
const GICD_SGIR_TARGET_OTHERS = (1 << 24); // send to all CPUs except self
const GICD_SGIR_TARGET_SELF = (2 << 24);   // send to self only

// GICD_SGIR — SGI IDs (bits 3:0); IDs 0–15 are reserved for software use
const GICD_SGIR_IRQ_0 = 0; // yield / context switch
const GICD_SGIR_IRQ_1 = 1;
const GICD_SGIR_IRQ_2 = 2;
const GICD_SGIR_IRQ_3 = 3;
const GICD_SGIR_IRQ_4 = 4;
const GICD_SGIR_IRQ_5 = 5;
const GICD_SGIR_IRQ_6 = 6;
const GICD_SGIR_IRQ_7 = 7;
const GICD_SGIR_IRQ_8 = 8;
const GICD_SGIR_IRQ_9 = 9;
const GICD_SGIR_IRQ_10 = 10;
const GICD_SGIR_IRQ_11 = 11;
const GICD_SGIR_IRQ_12 = 12;
const GICD_SGIR_IRQ_13 = 13;
const GICD_SGIR_IRQ_14 = 14;
const GICD_SGIR_IRQ_15 = 15;

// GICD_IPRIORITYR — interrupt priority (per byte, lower value = higher priority)
const GICD_PRIORITY_HIGH = 0x00;     // highest priority
const GICD_PRIORITY_MID_HIGH = 0x40; // medium-high priority
const GICD_PRIORITY_MID = 0xA0;      // medium priority
const GICD_PRIORITY_LOW = 0xF0;      // low priority

// GICD_ITARGETSR — CPU target bitmask (OR to target multiple CPUs)
const GICD_TARGET_CPU0 = (1 << 0);
const GICD_TARGET_CPU1 = (1 << 1);
const GICD_TARGET_CPU2 = (1 << 2);
const GICD_TARGET_CPU3 = (1 << 3);
const GICD_TARGET_CPU4 = (1 << 4);
const GICD_TARGET_CPU5 = (1 << 5);
const GICD_TARGET_CPU6 = (1 << 6);
const GICD_TARGET_CPU7 = (1 << 7);
const GICD_TARGET_ALL = (GICD_TARGET_CPU0 | GICD_TARGET_CPU1 | GICD_TARGET_CPU2 | GICD_TARGET_CPU3 |
                         GICD_TARGET_CPU4 | GICD_TARGET_CPU5 | GICD_TARGET_CPU6 | GICD_TARGET_CPU7);

// GICC_IAR — interrupt acknowledge register masks
const GICC_IAR_IRQ_MASK = 0x3FF; // bits 9:0 — IRQ ID

// Memory-mapped GIC distributor and CPU interface register blocks
@static let GICD: struct gicd_regs*;
@static let GICC: struct gicc_regs*;

/**
 * Initializes the GIC distributor and CPU interface.
 * Enables the distributor, enables the CPU interface, and sets the priority
 * mask to accept all interrupt priority levels.
 */
fn gic_init() {
    GICD = GICD_BASE as struct gicd_regs*;
    GICC = GICC_BASE as struct gicc_regs*;
    GICD->ctlr = 1;  // Enable distributor
    GICC->ctlr = 1;   // Enable CPU interface
    GICC->pmr = 0xFF; // Accept all priority levels
}

/**
 * Enables a specific IRQ in the GIC distributor and routes it to CPU 0
 * at mid priority.
 *
 * @param irq: absolute GIC IRQ ID to enable
 */
fn gic_enable_irq(irq: uint32) {
    GICD->isenabler[irq / 32] = (1 << (irq % 32));
    GICD->ipriorityr[irq / 4] = GICD->ipriorityr[irq / 4] | (GICD_PRIORITY_MID << ((irq % 4) * 8));
    GICD->itargetsr[irq / 4] = GICD->itargetsr[irq / 4] | (GICD_TARGET_CPU0 << ((irq % 4) * 8));
}

/**
 * Triggers a Software Generated Interrupt on the current CPU by writing
 * to GICD->sgir with GICD_SGIR_TARGET_SELF.
 *
 * @param irq: SGI ID to trigger (0–15)
 */
fn gic_trigger_sgi(irq: uint32) {
    GICD->sgir = GICD_SGIR_TARGET_SELF | (irq & 0xF);
}

/**
 * Acknowledges the highest-priority pending interrupt and moves it to active state.
 * Must be called at the start of an IRQ handler before processing.
 * Returns 1023 if no real interrupt is pending (spurious).
 *
 * @return Absolute GIC IRQ ID of the acknowledged interrupt.
 */
fn gic_acknowledge() -> uint32 {
    return GICC->iar & GICC_IAR_IRQ_MASK;
}

/**
 * Signals end of interrupt to the GIC, allowing the next interrupt of equal
 * or lower priority to be delivered. Must be called at the end of every IRQ handler.
 *
 * @param irq: IRQ ID returned by gic_acknowledge()
 */
fn gic_end_of_interrupt(irq: uint32) {
    GICC->eoir = irq;
}
