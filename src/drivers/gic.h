#ifndef GIC_H
#define GIC_H

#include <types.h>

#define GICD_BASE 0x08000000UL
#define GICC_BASE 0x08010000UL

// Distributor registers
#define GICD_CTLR (*(volatile uint32_t *)(GICD_BASE + 0x000))
#define GICD_ISENABLER ((volatile uint32_t *)(GICD_BASE + 0x100))
#define GICD_IPRIORITYR ((volatile uint32_t *)(GICD_BASE + 0x400))
#define GICD_ITARGETSR ((volatile uint32_t *)(GICD_BASE + 0x800))
#define GICD_ICFGR ((volatile uint32_t *)(GICD_BASE + 0xC00))
#define GICD_SGIR (*(volatile uint32_t *)(GICD_BASE + 0xF00))

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
#define GICD_TARGET_ALL (GICD_TARGET_CPU0 | GICD_TARGET_CPU1 | \
                         GICD_TARGET_CPU2 | GICD_TARGET_CPU3 | \
                         GICD_TARGET_CPU4 | GICD_TARGET_CPU5 | \
                         GICD_TARGET_CPU6 | GICD_TARGET_CPU7)

// GICC_IAR — interrupt acknowledge register masks
#define GICC_IAR_IRQ_MASK 0x3FF // bits 9:0 — IRQ ID

// CPU interface registers
#define GICC_CTLR (*(volatile uint32_t *)(GICC_BASE + 0x000))
#define GICC_PMR (*(volatile uint32_t *)(GICC_BASE + 0x004))
#define GICC_IAR (*(volatile uint32_t *)(GICC_BASE + 0x00C))
#define GICC_EOIR (*(volatile uint32_t *)(GICC_BASE + 0x010))

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
 * to GICD_SGIR with GICD_SGIR_TARGET_SELF.
 *
 * @param irq: SGI ID to trigger (0–15)
 */
void gic_trigger_sgi(uint32_t irq);

/**
 * Voluntarily yields the CPU by triggering SGI 0 on the current CPU.
 * The SGI fires immediately, causing the scheduler to select the next
 * ready process. Only callable from EL1.
 */
void gic_yield();

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

#endif // GIC_H
