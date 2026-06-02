#include <dtb.h>
#include <debug.h>
#include <arch/cpu.h>
#include <arch/irq.h>
#include <arch/syscall.h>
#include <sched/scheduler.h>
#include "timer.h"
#include "gic.h"

static uint32_t timer_irq;

static struct sched_info tinfo = {.interval = DEFAULT_TIMER_INTERVAL};

#define _time_quanta (tinfo.frequency * tinfo.interval / 1000)
#define _time_quantum(n) (n * _time_quanta)

void timer_init()
{
    // Get frequency
    tinfo.initial_ticks = get_cntpct_el0();
    tinfo.frequency = get_cntfrq_el0();

    // Set timer countdown
    set_cntp_tval_el0(_time_quantum(1));

    // Enable timer, unmask interrupt
    set_cntp_ctl_el0(CNTP_CTL_ENABLE);

    // register uptime syscall
    syscall_register_handler(SYSCALL_UPTIME, &syscall_uptime_handler);

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

time_t timer_get_uptime()
{
    uint64_t ticks = get_cntpct_el0();
    return (ticks - tinfo.initial_ticks) * 1000 / tinfo.frequency;
}

struct cpu_context *timer_irq_handler(__attribute__((unused)) int irq, struct cpu_context *ctx)
{
    uint64_t last_ticks = tinfo.ticks;

    tinfo.ticks = get_cntpct_el0();
    tinfo.frequency = get_cntfrq_el0();

    time_t interval = (tinfo.ticks - last_ticks) * 1000 / tinfo.frequency;
    time_t uptime = (tinfo.ticks - tinfo.initial_ticks) * 1000 / tinfo.frequency;

    dprintk("[scheduler] interval = %d ms, uptime = %d ms\r\n", interval, uptime);

    struct cpu_context *next_ctx = scheduler_handler(ctx, interval);

    // Set timer countdown
    set_cntp_tval_el0(_time_quantum(1));

    return next_ctx;
}

struct cpu_context *syscall_uptime_handler(struct cpu_context *ctx)
{
    time_t uptime = timer_get_uptime();

    dprintk("[timer] uptime = %d ms\r\n", uptime);

    ctx->x0 = uptime;

    return ctx;
}

void timer_set_interval(time_t interval)
{
    tinfo.interval = interval;
}