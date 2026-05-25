#ifndef CPU_H
#define CPU_H

#include <types.h>

static inline void wfe() { __asm__ volatile("wfe"); }
static inline void wfi() { __asm__ volatile("wfi"); }

static inline void irq_enable() { __asm__ volatile("msr daifclr, #2"); }
static inline void irq_disable() { __asm__ volatile("msr daifset, #2"); }

/**
 * Halts the CPU in a low-power loop using wfi, waking only to service
 * interrupts before returning to sleep.
 */
void halt();

/**
 * Spins forever with interrupts disabled. Used for unrecoverable errors
 * where the system must stop completely.
 */
void hang();

/**
 * Reads the timer frequency register (cntfrq_el0).
 *
 * @return Timer frequency in Hz as set by firmware.
 */
uint64_t get_cntfrq_el0();

/**
 * Writes the EL1 physical timer value register (cntp_tval_el0).
 * Starts a countdown from value; the timer fires IRQ when it reaches zero.
 * Must be rewritten each tick to reload the countdown.
 *
 * @param value: number of timer ticks until the next interrupt
 */
void set_cntp_tval_el0(const uint64_t value);

// cntp_ctl_el0 bits
#define CNTP_CTL_ENABLE (1 << 0)  // Timer enabled
#define CNTP_CTL_IMASK (1 << 1)   // Interrupt masked (suppress IRQ)
#define CNTP_CTL_ISTATUS (1 << 2) // Interrupt status (read-only, 1 = condition met)

/**
 * Reads the EL1 physical timer control register (cntp_ctl_el0).
 * Check CNTP_CTL_ISTATUS to determine if the timer condition has been met.
 *
 * @return Current value of cntp_ctl_el0.
 */
uint64_t get_cntp_ctl_el0();

/**
 * Writes the EL1 physical timer control register (cntp_ctl_el0).
 * Use CNTP_CTL_ENABLE, CNTP_CTL_IMASK, and CNTP_CTL_ISTATUS flags.
 *
 * @param value: value to write (combination of CNTP_CTL_* flags)
 */
void set_cntp_ctl_el0(const uint64_t value);

#endif // CPU_H
