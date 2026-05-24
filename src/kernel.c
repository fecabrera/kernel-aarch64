#include "cpu.h"
#include "uart.h"
#include "gic.h"
#include "interrupts.h"
#include "timer.h"
#include "dtb.h"
#include "rtc.h"

extern uint64_t dtb_ptr;

void kernel_main()
{
    uart_puts("Kernel starting...\r\n");

    dtb_init((void *)dtb_ptr);
    dtb_dump();

    uint32_t uart_irq = dtb_get_uart_irq_number();
    uart_puts("UART IRQ: ");
    uart_put_uint(uart_irq);
    uart_puts("\r\n");

    uint32_t timer_irq = dtb_get_timer_irq_number();
    uart_puts("Timer IRQ: ");
    uart_put_uint(timer_irq);
    uart_puts("\r\n");

    uint32_t rtc_irq = dtb_get_rtc_irq_number();
    uart_puts("RTC IRQ: ");
    uart_put_uint(rtc_irq);
    uart_puts("\r\n");

    gic_init();
    interrupts_init();
    timer_init();
    uart_init();
    irq_enable();
    rtc_init();

    uart_puts("Interrupts enabled!\r\n");

    uint32_t timestamp = rtc_get_time();
    uart_puts("Current timestamp: ");
    uart_put_uint(timestamp);
    uart_puts("\r\n");

    halt();
}
