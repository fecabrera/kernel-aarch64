#include "rtc.h"

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