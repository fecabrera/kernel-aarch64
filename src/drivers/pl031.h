#pragma once

#include <stdint.h>

#define RTC_BASE 0x09010000UL

/**
 * PL031 RTC register block, memory-mapped at RTC_BASE. All registers are 32-bit
 * and laid out contiguously from offset 0x000; access them through the PL031
 * pointer (e.g. PL031->dr).
 */
struct pl031_regs {
    volatile uint32_t dr;   // 0x000 Data (current time, read-only)
    volatile uint32_t mr;   // 0x004 Match (alarm time)
    volatile uint32_t lr;   // 0x008 Load (set current time)
    volatile uint32_t cr;   // 0x00C Control
    volatile uint32_t imsc; // 0x010 Interrupt mask
    volatile uint32_t ris;  // 0x014 Raw interrupt status
    volatile uint32_t mis;  // 0x018 Masked interrupt status
    volatile uint32_t icr;  // 0x01C Interrupt clear
};

// Memory-mapped PL031 register block
#define PL031 ((struct pl031_regs *)RTC_BASE)

// CR flags
#define RTC_CR_EN (1 << 0) // RTC enable

// Interrupt bits (same bit position in IMSC, RIS, MIS, ICR)
#define RTC_INT_MATCH (1 << 0) // Match interrupt

/**
 * Enables the RTC by setting the EN bit in CR, reads the IRQ number from the
 * DTB, registers pl031_irq_handler with the GIC, and registers syscall_time_handler for
 * SYSCALL_TIME.
 */
void pl031_init();

/**
 * IRQ handler for the RTC match interrupt.
 * Clears the interrupt flag and re-masks the interrupt until the next alarm is set.
 * Registered with irq_register_handler at rtc_init time.
 *
 * @param irq: IRQ ID passed by the dispatcher (unused)
 * @param ctx: saved register frame from the interrupted context
 *
 * @return ctx unchanged (no context switch).
 */
struct cpu_context *pl031_irq_handler(int irq, struct cpu_context *ctx);

/**
 * Reads the current time from the RTC data register.
 *
 * @return Current Unix timestamp.
 */
uint32_t pl031_get_time();

/**
 * Sets the current time by writing to the RTC load register.
 *
 * @param unix_time: Unix timestamp to set
 */
void pl031_set_time(uint32_t unix_time);

/**
 * Sets an alarm to fire at the given Unix timestamp.
 * Writes to the match register, unmasks the match interrupt, and enables the GIC IRQ.
 *
 * @param unix_time: Unix timestamp at which the alarm should fire
 */
void pl031_set_alarm(uint32_t unix_time);

/**
 * Syscall handler for SYSCALL_TIME. Reads the current Unix timestamp from
 * the RTC data register (PL031->dr) and writes it into ctx->x0. Does not
 * perform a context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx unchanged, with ctx->x0 set to the current Unix timestamp
 */
struct cpu_context *syscall_time_handler(struct cpu_context *ctx);
