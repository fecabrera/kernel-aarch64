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
// EC occupies bits [31:26] of ESR_EL1; ISS occupies bits [24:0].
#define ESR_EC_SHIFT 26
#define ESR_EC_MASK  0x3F

// EC values (ARMv8-A Architecture Reference Manual, Table D17-2)
#define ESR_EC_UNKNOWN      0x00 // Unknown reason
#define ESR_EC_WFx          0x01 // Trapped WFI or WFE instruction
#define ESR_EC_FP_ACCESS    0x07 // Trapped access to SVE/SIMD/FP
#define ESR_EC_ILL_STATE    0x0E // SP alignment fault (SP not 16-byte aligned)
#define ESR_EC_FP_EXC       0x2C // Trapped floating-point exception
#define ESR_EC_SVC64        0x15 // SVC executed in AArch64 (syscall entry)
#define ESR_EC_IABT_EL0     0x20 // Instruction abort from EL0 (page fault)
#define ESR_EC_IABT_EL1     0x21 // Instruction abort from EL1
#define ESR_EC_PC_ALIGN     0x22 // PC alignment fault (PC not 4-byte aligned)
#define ESR_EC_DABT_EL0     0x24 // Data abort from EL0 (page fault / bus error)
#define ESR_EC_DABT_EL1     0x25 // Data abort from EL1
#define ESR_EC_SERROR       0x2F // SError interrupt (async system error)

#define IRQ_PPI_EL2_HYPERVISOR_TIMER 27 // PPI: EL2 hypervisor physical timer
#define IRQ_PPI_EL1_VIRTUAL_TIMER 28    // PPI: EL1 virtual timer
#define IRQ_PPI_EL3_SECURE_TIMER 29     // PPI: EL3 secure physical timer
#define IRQ_PPI_EL1_PHYSICAL_TIMER 30   // PPI: EL1 physical timer
#define IRQ_SPI_PL011_UART0 32          // SPI: PL011 UART0
#define IRQ_SPI_PL011_UART1 33          // SPI: PL011 UART1
#define IRQ_SPI_PL031_RTC_ALARM 34      // SPI: PL011 RTC alarm

#define NUM_IRQS 256

typedef struct cpu_context *(*interrupt_handler)(struct cpu_context *);

/**
 * Installs the exception vector table by writing its address to vbar_el1,
 * followed by an ISB to ensure the CPU sees the new table immediately.
 */
void irq_init();

/**
 * Registers a handler for the given GIC IRQ ID.
 *
 * @param irq: GIC IRQ ID (0–255)
 * @param fnc: function to call when the interrupt fires
 *
 * @return 0 on success, -1 if a handler is already registered for that IRQ
 */
int irq_register_handler(uint32_t irq, interrupt_handler fnc);

/**
 * Removes the handler for the given GIC IRQ ID.
 * After this call, unhandled interrupts for that ID will log a warning.
 *
 * @param irq: GIC IRQ ID (0–255)
 *
 * @return 0 on success, -1 if no handler was registered for that IRQ
 */
int irq_unregister_handler(uint32_t irq);

/**
 * Handles synchronous exceptions (data aborts, instruction aborts, syscalls).
 * Decodes the exception class from ESR_EL1 and dispatches accordingly.
 * For syscalls (ESR_EC_SVC64), may return a different context to perform
 * a context switch. For faults, hangs or returns ctx unchanged.
 *
 * @param ctx: pointer to the saved cpu_context on the current task's stack
 * @param esr: Exception Syndrome Register value at the time of the exception
 * @param elr: Exception Link Register — address of the faulting instruction
 * @param far: Fault Address Register — virtual address that caused the fault
 *
 * @return pointer to the saved cpu_context of the next task to run
 */
struct cpu_context *sync_handler(struct cpu_context *ctx, uint64_t esr, uint64_t elr, uint64_t far);

/**
 * Handles IRQ exceptions. Acknowledges the interrupt from the GIC,
 * dispatches to the registered handler in irq_table, then signals end of interrupt.
 * Called from the IRQ vector stub in vectors.S with the saved register frame.
 *
 * @param ctx: pointer to the saved cpu_context on the current task's stack
 *
 * @return pointer to the saved cpu_context of the next task to run;
 *         the assembly stub writes this into sp before restore_context and eret.
 */
struct cpu_context *irq_handler(struct cpu_context *ctx);

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
