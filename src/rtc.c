#include "rtc.h"
#include "irq.h"
#include "gic.h"
#include "uart.h"

void rtc_init()
{
    RTC_CR = RTC_CR_EN;
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
    gic_enable_irq(IRQ_SPI_PL031_RTC_ALARM);
}

void rtc_irq_handler()
{
    uart_puts("[rtc] alarm fired!\r\n");
    RTC_ICR = RTC_INT_MATCH;   // clear interrupt
    RTC_IMSC = ~RTC_INT_MATCH; // mask
}