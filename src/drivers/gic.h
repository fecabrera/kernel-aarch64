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
