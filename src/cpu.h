#ifndef CPU_H
#define CPU_H

#include <types.h>

static inline void wfe() { __asm__ volatile("wfe"); }
static inline void wfi() { __asm__ volatile("wfi"); }

static inline void irq_enable() { __asm__ volatile("msr daifclr, #2"); }
static inline void irq_disable() { __asm__ volatile("msr daifset, #2"); }

void halt();

uint64_t get_cntfrq_el0();
void set_cntp_tval_el0(const uint64_t value);

// cntp_ctl_el0 bits
#define CNTP_CTL_ENABLE (1 << 0)  // Timer enabled
#define CNTP_CTL_IMASK (1 << 1)   // Interrupt masked (suppress IRQ)
#define CNTP_CTL_ISTATUS (1 << 2) // Interrupt status (read-only, 1 = condition met)

uint64_t get_cntp_ctl_el0();
void set_cntp_ctl_el0(const uint64_t value);

#endif // CPU_H
