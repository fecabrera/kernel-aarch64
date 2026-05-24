#include "cpu.h"
#include "uart.h"
#include "gic.h"
#include "irq.h"
#include "timer.h"
#include "dtb.h"
#include "rtc.h"

extern void *dtb_ptr;

void kernel_main()
{
    uart_puts("Kernel starting...\r\n");

    dtb_init(dtb_ptr);
    dtb_dump();

    gic_init();
    interrupts_init();
    timer_init();
    uart_init();
    rtc_init();
    irq_enable();

    uart_puts("Interrupts enabled!\r\n");

    uint32_t timestamp = rtc_get_time();
    uart_puts("Current timestamp: ");
    uart_put_uint(timestamp);
    uart_puts("\r\n");

    rtc_set_alarm(timestamp + 1);

    halt();
}
