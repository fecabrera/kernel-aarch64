#include "timer.h"
#include "gic.h"
#include "interrupts.h"
#include "cpu.h"

static volatile uint64_t ticks = 0;

void timer_init()
{
    // Get frequency
    uint64_t freq = get_cntfrq_el0();

    // Set timer countdown
    set_cntp_tval_el0((freq * TIMER_INTERVAL_MS) / 1000);

    // Enable timer, unmask interrupt
    set_cntp_ctl_el0(CNTP_CTL_ENABLE);

    gic_enable_irq(IRQ_PPI_EL1_PHYSICAL_TIMER);
}

void timer_handler()
{
    ticks++;

    // Get frequency
    uint64_t freq = get_cntfrq_el0();

    // Set timer countdown
    set_cntp_tval_el0((freq * TIMER_INTERVAL_MS) / 1000);
}