#include "pl031.h"
#include "gic.h"
#include <arch/irq.h>
#include <arch/syscall.h>
#include <debug.h>
#include <dtb.h>

static uint32_t pl031_irq;

void pl031_init() {
    RTC_CR = RTC_CR_EN;

    if (dtb_get_rtc_irq_number(&pl031_irq) == 0) {
        dprintk("[pl031] Initializing IRQ: %i\n", pl031_irq);
        irq_register_handler(pl031_irq, &pl031_irq_handler);
        syscall_register_handler(SYSCALL_TIME, &time_handler);
    } else {
        dprintk("[pl031] IRQ not found!!\n");
    }
}

uint32_t pl031_get_time() { return RTC_DR; }

void pl031_set_time(uint32_t unix_time) { RTC_LR = unix_time; }

void pl031_set_alarm(uint32_t unix_time) {
    RTC_MR = unix_time;
    RTC_IMSC = RTC_INT_MATCH;

    gic_enable_irq(pl031_irq);
}

struct cpu_context *pl031_irq_handler(__attribute__((unused)) int irq, struct cpu_context *ctx) {
    dprintk("[pl031] alarm fired!\n");

    RTC_ICR = RTC_INT_MATCH;   // clear interrupt
    RTC_IMSC = ~RTC_INT_MATCH; // mask

    return ctx;
}

struct cpu_context *time_handler(struct cpu_context *ctx) {
    dprintk("[pl031] time()\n");

    ctx->x0 = pl031_get_time();
    return ctx;
}