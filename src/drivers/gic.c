#include "gic.h"

void gic_init()
{
    GICD_CTLR = 1;   // Enable distributor
    GICC_CTLR = 1;   // Enable CPU interface
    GICC_PMR = 0xFF; // Accept all priority levels
}

void gic_enable_irq(uint32_t irq)
{
    GICD_ISENABLER[irq / 32] = (1 << (irq % 32));
    GICD_IPRIORITYR[irq / 4] |= (GICD_PRIORITY_MID << ((irq % 4) * 8));
    GICD_ITARGETSR[irq / 4] |= (GICD_TARGET_CPU0 << ((irq % 4) * 8));
}

void gic_trigger_sgi(uint32_t irq)
{
    GICD_SGIR = GICD_SGIR_TARGET_SELF | (irq & 0xF);
}

uint32_t gic_acknowledge()
{
    return GICC_IAR & GICC_IAR_IRQ_MASK;
}

void gic_end_of_interrupt(uint32_t irq)
{
    GICC_EOIR = irq;
}