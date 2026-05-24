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
    GICD_IPRIORITYR[irq / 4] |= (0xA0 << ((irq % 4) * 8)); // Mid priority
    GICD_ITARGETSR[irq / 4] |= (0x01 << ((irq % 4) * 8));  // Route to CPU 0
}

uint32_t gic_acknowledge()
{
    return GICC_IAR & 0x3FF; // Lower 10 bits = IRQ ID
}

void gic_end_of_interrupt(uint32_t irq)
{
    GICC_EOIR = irq;
}