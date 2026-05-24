#ifndef RTC_H
#define RTC_H

#include <types.h>

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

void rtc_init();
void rtc_irq_handler();

uint32_t rtc_get_time();
void rtc_set_time(uint32_t unix_time);
void rtc_set_alarm(uint32_t unix_time);

#endif // RTC_H