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

void gic_init();
void gic_enable_irq(uint32_t irq);
uint32_t gic_acknowledge();
void gic_end_of_interrupt(uint32_t irq);

#endif // GIC_H
