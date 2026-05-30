#include <dtb.h>
#include <debug.h>
#include <arch/irq.h>
#include <arch/syscall.h>
#include "rtc.h"
#include "gic.h"

uint32_t rtc_irq;

void rtc_init()
{
    RTC_CR = RTC_CR_EN;

    if (dtb_get_rtc_irq_number(&rtc_irq) == 0)
    {
        dprintk("[rtc] Initializing IRQ: %i\r\n", rtc_irq);
        irq_register_handler(rtc_irq, &rtc_irq_handler);
        syscall_register_handler(SYSCALL_TIME, &time_handler);
    }
    else
    {
        dprintk("[rtc] IRQ not found!!\r\n");
    }
}

uint32_t rtc_get_time()
{
    return RTC_DR;
}

void rtc_set_time(uint32_t unix_time)
{
    RTC_LR = unix_time;
}

void rtc_set_alarm(uint32_t unix_time)
{
    RTC_MR = unix_time;
    RTC_IMSC = RTC_INT_MATCH;

    gic_enable_irq(rtc_irq);
}

struct cpu_context *rtc_irq_handler(int irq, struct cpu_context *ctx)
{
    dprintk("[rtc] alarm fired!\r\n");

    RTC_ICR = RTC_INT_MATCH;   // clear interrupt
    RTC_IMSC = ~RTC_INT_MATCH; // mask

    return ctx;
}

struct cpu_context *time_handler(struct cpu_context *ctx)
{
    dprintk("[scheduler] time()\r\n");

    ctx->x0 = rtc_get_time();
    return ctx;
}