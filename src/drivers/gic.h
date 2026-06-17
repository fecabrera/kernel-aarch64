#pragma once

#include <stdint.h>

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

/**
 * GIC distributor register block, memory-mapped at GICD_BASE. The named
 * registers sit at fixed offsets with reserved gaps between them; the banked
 * registers (isenabler, ipriorityr, itargetsr, icfgr) are word arrays indexed
 * by IRQ ID. Access through the GICD pointer (e.g. GICD->ctlr).
 */
struct gicd_regs {
    volatile uint32_t ctlr;                   // 0x000 Distributor control
    uint32_t _reserved0[(0x100 - 0x004) / 4]; // 0x004–0x0FC
    volatile uint32_t isenabler[32];          // 0x100 Set-enable (1 bit/IRQ)
    uint32_t _reserved1[(0x400 - 0x180) / 4]; // 0x180–0x3FC
    volatile uint32_t ipriorityr[256];        // 0x400 Priority (1 byte/IRQ)
    volatile uint32_t itargetsr[256];         // 0x800 CPU targets (1 byte/IRQ)
    volatile uint32_t icfgr[64];              // 0xC00 Config (2 bits/IRQ)
    uint32_t _reserved2[(0xF00 - 0xD00) / 4]; // 0xD00–0xEFC
    volatile uint32_t sgir;                   // 0xF00 Software-generated interrupt
};

// Memory-mapped GIC distributor register block
#define GICD ((struct gicd_regs *)GICD_BASE)

// GICD_SGIR — target list filter (bits 25:24)
#define GICD_SGIR_TARGET_LIST (0b00 << 24)   // send to CPUs in target list (bits 19:16)
#define GICD_SGIR_TARGET_OTHERS (0b01 << 24) // send to all CPUs except self
#define GICD_SGIR_TARGET_SELF (0b10 << 24)   // send to self only

// GICD_SGIR — SGI IDs (bits 3:0); IDs 0–15 are reserved for software use
#define GICD_SGIR_IRQ_0 0 // yield / context switch
#define GICD_SGIR_IRQ_1 1
#define GICD_SGIR_IRQ_2 2
#define GICD_SGIR_IRQ_3 3
#define GICD_SGIR_IRQ_4 4
#define GICD_SGIR_IRQ_5 5
#define GICD_SGIR_IRQ_6 6
#define GICD_SGIR_IRQ_7 7
#define GICD_SGIR_IRQ_8 8
#define GICD_SGIR_IRQ_9 9
#define GICD_SGIR_IRQ_10 10
#define GICD_SGIR_IRQ_11 11
#define GICD_SGIR_IRQ_12 12
#define GICD_SGIR_IRQ_13 13
#define GICD_SGIR_IRQ_14 14
#define GICD_SGIR_IRQ_15 15

// GICD_IPRIORITYR — interrupt priority (per byte, lower value = higher priority)
#define GICD_PRIORITY_HIGH 0x00     // highest priority
#define GICD_PRIORITY_MID_HIGH 0x40 // medium-high priority
#define GICD_PRIORITY_MID 0xA0      // medium priority
#define GICD_PRIORITY_LOW 0xF0      // low priority

// GICD_ITARGETSR — CPU target bitmask (OR to target multiple CPUs)
#define GICD_TARGET_CPU0 (1 << 0)
#define GICD_TARGET_CPU1 (1 << 1)
#define GICD_TARGET_CPU2 (1 << 2)
#define GICD_TARGET_CPU3 (1 << 3)
#define GICD_TARGET_CPU4 (1 << 4)
#define GICD_TARGET_CPU5 (1 << 5)
#define GICD_TARGET_CPU6 (1 << 6)
#define GICD_TARGET_CPU7 (1 << 7)
#define GICD_TARGET_ALL                                                                            \
    (GICD_TARGET_CPU0 | GICD_TARGET_CPU1 | GICD_TARGET_CPU2 | GICD_TARGET_CPU3 |                   \
     GICD_TARGET_CPU4 | GICD_TARGET_CPU5 | GICD_TARGET_CPU6 | GICD_TARGET_CPU7)

// GICC_IAR — interrupt acknowledge register masks
#define GICC_IAR_IRQ_MASK 0x3FF // bits 9:0 — IRQ ID

/**
 * GIC CPU interface register block, memory-mapped at GICC_BASE. Registers are
 * 32-bit; a one-word gap at 0x008 (the unused BPR) separates pmr from iar.
 * Access through the GICC pointer (e.g. GICC->iar).
 */
struct gicc_regs {
    volatile uint32_t ctlr; // 0x000 CPU interface control
    volatile uint32_t pmr;  // 0x004 Interrupt priority mask
    uint32_t _reserved0;    // 0x008 (BPR, unused)
    volatile uint32_t iar;  // 0x00C Interrupt acknowledge
    volatile uint32_t eoir; // 0x010 End of interrupt
};

// Memory-mapped GIC CPU interface register block
#define GICC ((struct gicc_regs *)GICC_BASE)

/**
 * Initializes the GIC distributor and CPU interface.
 * Enables the distributor, enables the CPU interface, and sets the priority
 * mask to accept all interrupt priority levels.
 */
void gic_init();

/**
 * Enables a specific IRQ in the GIC distributor and routes it to CPU 0
 * at mid priority.
 *
 * @param irq: absolute GIC IRQ ID to enable
 */
void gic_enable_irq(uint32_t irq);

/**
 * Triggers a Software Generated Interrupt on the current CPU by writing
 * to GICD->sgir with GICD_SGIR_TARGET_SELF.
 *
 * @param irq: SGI ID to trigger (0–15)
 */
void gic_trigger_sgi(uint32_t irq);

/**
 * Acknowledges the highest-priority pending interrupt and moves it to active state.
 * Must be called at the start of an IRQ handler before processing.
 * Returns 1023 if no real interrupt is pending (spurious).
 *
 * @return Absolute GIC IRQ ID of the acknowledged interrupt.
 */
uint32_t gic_acknowledge();

/**
 * Signals end of interrupt to the GIC, allowing the next interrupt of equal
 * or lower priority to be delivered. Must be called at the end of every IRQ handler.
 *
 * @param irq: IRQ ID returned by gic_acknowledge()
 */
void gic_end_of_interrupt(uint32_t irq);
