#ifndef IRQ_H
#define IRQ_H

#include <types.h>

struct cpu_context
{
    uint64_t x0, x1;   // [sp, #0]
    uint64_t x2, x3;   // [sp, #16]
    uint64_t x4, x5;   // [sp, #32]
    uint64_t x6, x7;   // [sp, #48]
    uint64_t x8, x9;   // [sp, #64]
    uint64_t x10, x11; // [sp, #80]
    uint64_t x12, x13; // [sp, #96]
    uint64_t x14, x15; // [sp, #112]
    uint64_t x16, x17; // [sp, #128]
    uint64_t x18, x19; // [sp, #144]
    uint64_t x20, x21; // [sp, #160]
    uint64_t x22, x23; // [sp, #176]
    uint64_t x24, x25; // [sp, #192]
    uint64_t x26, x27; // [sp, #208]
    uint64_t x28, x29; // [sp, #224]
    uint64_t lr, elr;  // [sp, #240]
    uint64_t spsr;     // [sp, #256]
};

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

#define NUM_IRQS 256

typedef void (*interrupt_handler)(struct cpu_context *);

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
 * Registers a handler for the given GIC IRQ ID.
 * Overwrites any previously registered handler for that ID.
 *
 * @param irq: GIC IRQ ID (0–255)
 * @param fnc: function to call when the interrupt fires
 */
void irq_register_handler(uint32_t irq, interrupt_handler fnc);

/**
 * Removes the handler for the given GIC IRQ ID.
 * After this call, unhandled interrupts for that ID will log a warning.
 *
 * @param irq: GIC IRQ ID (0–255)
 */
void irq_unregister_handler(uint32_t irq);

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
