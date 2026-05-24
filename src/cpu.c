#include "cpu.h"
#include <types.h>

uint64_t get_cntfrq_el0()
{
    uint64_t freq;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

void set_cntp_tval_el0(const uint64_t value)
{
    __asm__ volatile("msr cntp_tval_el0, %0" ::"r"(value));
}

uint64_t get_cntp_ctl_el0()
{
    uint64_t reg;
    __asm__ volatile("mrs %0, cntp_ctl_el0" : "=r"(reg));
    return reg;
}

void set_cntp_ctl_el0(const uint64_t value)
{
    __asm__ volatile("msr cntp_ctl_el0, %0" ::"r"(value));
}