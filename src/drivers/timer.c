#include <dtb.h>
#include <debug.h>
#include <arch/cpu.h>
#include <arch/irq.h>
#include <sched/scheduler.h>
#include "timer.h"
#include "gic.h"

static uint32_t timer_irq;
static time_t timer_freq;
static time_t timer_interval = DEFAULT_TIMER_INTERVAL;

#define _time_quanta (timer_freq * timer_interval / 1000)
#define _time_quantum(n) (n * _time_quanta)

void timer_init()
{
    // Get frequency
    timer_freq = get_cntfrq_el0();

    // Set timer countdown
    set_cntp_tval_el0(_time_quantum(1));

    // Enable timer, unmask interrupt
    set_cntp_ctl_el0(CNTP_CTL_ENABLE);

    if (dtb_get_timer_irq_number(&timer_irq) == 0)
    {
        dprintk("[timer] Initializing IRQ: %i\r\n", timer_irq);
        irq_register_handler(timer_irq, &timer_irq_handler);
        gic_enable_irq(timer_irq);
    }
    else
    {
        dprintk("[timer] IRQ not found!!\r\n");
    }
}

struct cpu_context *timer_irq_handler(__attribute__((unused)) int irq, struct cpu_context *ctx)
{
    struct cpu_context *next_ctx = scheduler_handler(ctx, timer_interval);

    // Set timer countdown
    set_cntp_tval_el0(_time_quantum(1));

    return next_ctx;
}

void timer_set_interval(time_t interval)
{
    timer_interval = interval;
}