#include "timer.h"
#include "gic.h"
#include "irq.h"
#include "cpu.h"
#include "dtb.h"
#include "uart.h"

static volatile uint64_t ticks = 0;

uint32_t timer_irq;
uint64_t timer_freq, timer_interval;

void timer_init()
{
    // Get frequency
    timer_freq = get_cntfrq_el0();

    // Set timer countdown
    set_cntp_tval_el0((timer_freq * timer_interval) / 1000);

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
        uart_puts("Timer IRQ not found!!\r\n");
    }
}

void timer_handler()
{
    ticks++;

    if (((timer_interval * ticks) % 1000) == 0)
    {
        uart_puts("[timer] 1s elapsed\r\n");
    }

    // Set timer countdown
    set_cntp_tval_el0((timer_freq * timer_interval) / 1000);
}

void timer_set_interval(uint64_t interval)
{
    timer_interval = interval;
}