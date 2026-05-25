#ifndef IRQ_H
#define IRQ_H

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
#define IRQ_SPI_PL031_RTC_ALARM 34      // SPI: PL011 RTC alarm

/**
 * Installs the exception vector table by writing its address to vbar_el1,
 * followed by an ISB to ensure the CPU sees the new table immediately.
 */
void interrupts_init();

/**
 * Handles synchronous exceptions (data aborts, instruction aborts, syscalls).
 * Decodes the exception class from ESR_EL1 and dispatches accordingly.
 *
 * @param esr: Exception Syndrome Register value at the time of the exception
 * @param elr: Exception Link Register — address of the faulting instruction
 * @param far: Fault Address Register — virtual address that caused the fault
 * @param ctx: optional context pointer (unused)
 */
void sync_handler(uint64_t esr, uint64_t elr, uint64_t far, void *ctx);

/**
 * Handles IRQ exceptions. Acknowledges the interrupt from the GIC,
 * dispatches to the appropriate device handler, then signals end of interrupt.
 *
 * @param ctx: optional context pointer (unused)
 */
void irq_handler(void *ctx);

/**
 * Handles FIQ exceptions. Currently logs and returns.
 */
void fiq_handler();

/**
 * Handles SError (System Error) exceptions. Logs and halts — SErrors are
 * typically unrecoverable hardware faults.
 */
void serr_handler();

#endif // IRQ_H
