#pragma once

#include <stdint.h>

#define RTC_BASE 0x09010000UL

#define RTC_DR (*(volatile uint32_t *)(RTC_BASE + 0x000))   // Data (current time, read-only)
#define RTC_MR (*(volatile uint32_t *)(RTC_BASE + 0x004))   // Match (alarm time)
#define RTC_LR (*(volatile uint32_t *)(RTC_BASE + 0x008))   // Load (set current time)
#define RTC_CR (*(volatile uint32_t *)(RTC_BASE + 0x00C))   // Control
#define RTC_IMSC (*(volatile uint32_t *)(RTC_BASE + 0x010)) // Interrupt mask
#define RTC_RIS (*(volatile uint32_t *)(RTC_BASE + 0x014))  // Raw interrupt status
#define RTC_MIS (*(volatile uint32_t *)(RTC_BASE + 0x018))  // Masked interrupt status
#define RTC_ICR (*(volatile uint32_t *)(RTC_BASE + 0x01C))  // Interrupt clear

// CR flags
#define RTC_CR_EN (1 << 0) // RTC enable

// Interrupt bits (same bit position in IMSC, RIS, MIS, ICR)
#define RTC_INT_MATCH (1 << 0) // Match interrupt

/**
 * Enables the RTC by setting the EN bit in CR, reads the IRQ number from the
 * DTB, registers rtc_irq_handler with the GIC, and registers time_handler for
 * SYSCALL_TIME.
 */
void rtc_init();

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
struct cpu_context *rtc_irq_handler(int irq, struct cpu_context *ctx);

/**
 * Reads the current time from the RTC data register.
 *
 * @return Current Unix timestamp.
 */
uint32_t rtc_get_time();

/**
 * Sets the current time by writing to the RTC load register.
 *
 * @param unix_time: Unix timestamp to set
 */
void rtc_set_time(uint32_t unix_time);

/**
 * Sets an alarm to fire at the given Unix timestamp.
 * Writes to the match register, unmasks the match interrupt, and enables the GIC IRQ.
 *
 * @param unix_time: Unix timestamp at which the alarm should fire
 */
void rtc_set_alarm(uint32_t unix_time);

/**
 * Syscall handler for SYSCALL_TIME. Reads the current Unix timestamp from
 * the RTC data register (RTC_DR) and writes it into ctx->x0. Does not
 * perform a context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx unchanged, with ctx->x0 set to the current Unix timestamp
 */
struct cpu_context *time_handler(struct cpu_context *ctx);
