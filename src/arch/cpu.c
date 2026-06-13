#include "cpu.h"
#include <stdbool.h>
#include <stdint.h>

void wfe() { __asm__ volatile("wfe"); }
void wfi() { __asm__ volatile("wfi"); }

void irq_enable() { __asm__ volatile("msr daifclr, #2"); }
void irq_disable() { __asm__ volatile("msr daifset, #2"); }

uint64_t get_cntpct_el0() {
    uint64_t count;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(count));
    return count;
}

uint64_t get_cntfrq_el0() {
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

void set_cntp_tval_el0(const uint64_t value) {
    __asm__ volatile("msr cntp_tval_el0, %0" ::"r"(value));
}

uint64_t get_cntp_ctl_el0() {
    uint64_t reg;
    __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(reg));
    return reg;
}

void set_cntp_ctl_el0(const uint64_t value) {
    __asm__ volatile("msr cntp_ctl_el0, %0" ::"r"(value));
}

void halt() {
    _wfi_while(true);
    __builtin_unreachable();
}

void hang() {
    _wfe_while(true);
    __builtin_unreachable();
}