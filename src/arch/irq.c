#include <drivers/uart.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <drivers/rtc.h>
#include "irq.h"
#include "cpu.h"

extern void vector_table();

// ── Install vector table ────────────────────────────

void interrupts_init()
{
    // Point CPU to our vector table
    __asm__ volatile(
        "msr vbar_el1, %0\n"
        "isb\n" // Instruction sync barrier
        ::"r"(vector_table));
}

// ── ESR_EL1 — decode what caused a sync exception ──

void sync_handler(uint64_t esr, uint64_t elr, uint64_t far, void *ctx)
{
    uint32_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

    switch (ec)
    {
    case ESR_EC_SVC64:
        uart_puts("[SYNC] Syscall!\r\n");
        break;
    case ESR_EC_DABT_EL0:
        uart_puts("[SYNC] Data abort!\r\n");
        break;
    default:
        uart_puts("[SYNC] Unknown exception\r\n");
        wfe();
    }
}

void serr_handler()
{
    uart_puts("[SError] System error!\r\n");
    while (1)
        ;
}

void fiq_handler()
{
    uart_puts("[FIQ]\r\n");
}

// ── IRQ handler ─────────────────────────────────────

void irq_handler(void *ctx)
{
    // Ask the GIC which interrupt fired (see below)
    uint32_t irq_id = gic_acknowledge();

    switch (irq_id)
    {
    case IRQ_PPI_EL1_PHYSICAL_TIMER:
        timer_handler();
        break;
    case IRQ_SPI_PL011_UART0:
    case IRQ_SPI_PL011_UART1:
        uart_handler();
        break;
    case IRQ_SPI_PL031_RTC_ALARM:
        rtc_irq_handler();
        break;
    default:
        uart_puts("[IRQ] Unknown\r\n");
    }

    gic_end_of_interrupt(irq_id);
}