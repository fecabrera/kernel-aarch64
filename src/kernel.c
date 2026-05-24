#include "cpu.h"
#include "uart.h"
#include "gic.h"
#include "interrupts.h"
#include "timer.h"
#include "dtb.h"

extern uint64_t dtb_ptr;

void kernel_main()
{
    uart_puts("Kernel starting...\r\n");

    dtb_init((void *)dtb_ptr);

    gic_init();
    interrupts_init();
    timer_init();
    uart_init();
    irq_enable();

    uart_puts("Interrupts enabled!\r\n");

    while (1)
    {
        wfi();
    }
}
