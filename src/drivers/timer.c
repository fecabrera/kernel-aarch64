#include <dtb.h>
#include <arch/cpu.h>
#include <arch/irq.h>
#include "timer.h"
#include "gic.h"
#include "uart.h"

static volatile uint64_t ticks = 0;

uint32_t timer_irq;
uint64_t timer_freq;
uint64_t timer_interval = DEFAULT_TIMER_INTERVAL;

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

        irq_register_handler(timer_irq, &timer_irq_handler);
        gic_enable_irq(timer_irq);
    }
    else
    {
        uart_puts("Timer IRQ not found!!\r\n");
    }
}

struct cpu_context *timer_irq_handler(struct cpu_context *ctx)
{
    if (ticks++ == 0)
    {
        uart_puts("[timer] first tick!\r\n");
    }

    // Set timer countdown
    set_cntp_tval_el0((timer_freq * timer_interval) / 1000);

    return ctx;
}

void timer_set_interval(uint64_t interval)
{
    timer_interval = interval;
}