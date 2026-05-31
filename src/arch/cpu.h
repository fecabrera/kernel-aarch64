#pragma once

#include <stdint.h>

// Endian conversion macros. On a little-endian host (AArch64 in LE mode),
// _be32/_be64 byte-swap to/from big-endian (e.g. DTB fields);
// _le32/_le64 are no-ops. Reversed on a big-endian host.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define _be16(x) __builtin_bswap16(x)
#define _be32(x) __builtin_bswap32(x)
#define _be64(x) __builtin_bswap64(x)
#define _le16(x) x
#define _le32(x) x
#define _le64(x) x
#else
#define _be16(x) x
#define _be32(x) x
#define _be64(x) x
#define _le16(x) __builtin_bswap16(x)
#define _le32(x) __builtin_bswap32(x)
#define _le64(x) __builtin_bswap64(x)
#endif

static inline void wfe() { __asm__ volatile("wfe"); }
static inline void wfi() { __asm__ volatile("wfi"); }

static inline void irq_enable() { __asm__ volatile("msr daifclr, #2"); }
static inline void irq_disable() { __asm__ volatile("msr daifset, #2"); }

/**
 * Halts the CPU in a low-power loop using wfi, waking only to service
 * interrupts before returning to sleep.
 */
void halt() __attribute__((noreturn));

/**
 * Spins forever with interrupts disabled. Used for unrecoverable errors
 * where the system must stop completely.
 */
void hang() __attribute__((noreturn));

/**
 * Reads the physical count register (cntpct_el0).
 * Increments at the frequency reported by cntfrq_el0 (typically 24 MHz on
 * QEMU virt). Divide by get_cntfrq_el0() to convert ticks to seconds.
 *
 * @return Current 64-bit physical counter value.
 */
uint64_t get_cntpct_el0();

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

// SPSR_EL1 — exception level and stack pointer selection (bits 3:0)
#define SPSR_EL0t (0x00) // EL0, SP_EL0
#define SPSR_EL1t (0x04) // EL1, SP_EL0
#define SPSR_EL1h (0x05) // EL1, SP_EL1 (standard kernel mode)
#define SPSR_EL2t (0x08) // EL2, SP_EL0
#define SPSR_EL2h (0x09) // EL2, SP_EL2

// SPSR_EL1 — interrupt mask bits (DAIF, bits 9:6)
#define SPSR_D (1 << 9) // Debug exceptions masked
#define SPSR_A (1 << 8) // SError masked
#define SPSR_I (1 << 7) // IRQ masked
#define SPSR_F (1 << 6) // FIQ masked
#define SPSR_MASKS_ALL (SPSR_D | SPSR_A | SPSR_I | SPSR_F)

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
