#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <types.h>

// ESR_EL1 exception class fields
#define ESR_EC_SHIFT 26
#define ESR_EC_MASK 0x3F
#define ESR_EC_SVC64 0x15
#define ESR_EC_DABT_EL0 0x24
#define ESR_EC_IABT_EL0 0x20

#define IRQ_PPI_EL2_HYPERVISOR_TIMER 27 // PPI: EL2 hypervisor physical timer
#define IRQ_PPI_EL1_VIRTUAL_TIMER 28    // PPI: EL1 virtual timer
#define IRQ_PPI_EL3_SECURE_TIMER 29     // PPI: EL3 secure physical timer
#define IRQ_PPI_EL1_PHYSICAL_TIMER 30   // PPI: EL1 physical timer
#define IRQ_SPI_PL011_UART0 32          // SPI: PL011 UART0
#define IRQ_SPI_PL011_UART1 33          // SPI: PL011 UART1

void interrupts_init();

void sync_handler(uint64_t esr, uint64_t elr, uint64_t far, void *ctx);
void irq_handler(void *ctx);
void fiq_handler();
void serr_handler();

#endif // INTERRUPTS_H
