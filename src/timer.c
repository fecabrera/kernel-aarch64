#include "timer.h"
#include "gic.h"
#include "irq.h"
#include "cpu.h"
#include "dtb.h"
#include "uart.h"

static volatile uint64_t ticks = 0;

uint32_t timer_irq;

void timer_init()
{
    // Get frequency
    uint64_t freq = get_cntfrq_el0();

    // Set timer countdown
    set_cntp_tval_el0((freq * TIMER_INTERVAL_MS) / 1000);

    // Enable timer, unmask interrupt
    set_cntp_ctl_el0(CNTP_CTL_ENABLE);

    if (dtb_get_timer_irq_number(&timer_irq) == 0)
    {
        uart_puts("Timer IRQ: ");
        uart_put_uint(timer_irq);
        uart_puts("\r\n");

        gic_enable_irq(timer_irq);
    }
    else
    {
        uart_puts("Timer IRQ not found!!");
    }
}

void timer_handler()
{
    ticks++;

    // Get frequency
    uint64_t freq = get_cntfrq_el0();

    // Set timer countdown
    set_cntp_tval_el0((freq * TIMER_INTERVAL_MS) / 1000);
}