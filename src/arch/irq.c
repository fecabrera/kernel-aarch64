#include <string.h>
#include <drivers/uart.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <drivers/rtc.h>
#include "cpu.h"
#include "irq.h"
#include "syscall.h"

extern void vector_table();

interrupt_handler irq_table[NUM_IRQS] = {NULL};

// ── Install vector table ────────────────────────────

void irq_init()
{
    // Point CPU to our vector table
    __asm__ volatile(
        "msr vbar_el1, %0\n"
        "isb\n" // Instruction sync barrier
        ::"r"(vector_table));
}

// ── ESR_EL1 — decode what caused a sync exception ──

struct cpu_context *sync_handler(struct cpu_context *ctx, uint64_t esr, uint64_t elr, uint64_t far)
{
    uint32_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

    switch (ec)
    {
    case ESR_EC_SVC64:
        ctx = syscall_handler(ctx);
        break;
    case ESR_EC_DABT_EL0:
        uart_puts("[sync] Data abort!\r\n");
        break;
    default:
        uart_puts("[sync] Unknown exception\r\n");
        wfe();
    }

    return ctx;
}

void serr_handler()
{
    uart_puts("[SError] System error!\r\n");
    hang();
}

void fiq_handler()
{
    uart_puts("[FIQ]\r\n");
}

void irq_register_handler(uint32_t irq, interrupt_handler fnc)
{
    if (irq_table[irq] == NULL)
    {
        irq_table[irq] = fnc;
        uart_puts("[irq] Handler registered for IRQ ");
        uart_put_uint(irq);
        uart_puts("!\r\n");
    }
    else
    {
        uart_puts("[irq] There's already a handler registered for IRQ ");
        uart_put_uint(irq);
        uart_puts("!\r\n");
    }
}

void irq_unregister_handler(uint32_t irq)
{
    if (irq_table[irq] == NULL)
    {
        uart_puts("[irq] There's no handler registered for IRQ ");
        uart_put_uint(irq);
        uart_puts("!\r\n");
    }
    else
    {
        irq_table[irq] = NULL;
        uart_puts("[irq] Handler unregistered for IRQ ");
        uart_put_uint(irq);
        uart_puts("!\r\n");
    }
}

struct cpu_context *irq_handler(struct cpu_context *ctx)
{
    // Ask the GIC which interrupt fired (see below)
    uint32_t irq_id = gic_acknowledge();
    interrupt_handler fnc = irq_table[irq_id];

    if (fnc == NULL)
    {
        uart_puts("[irq] Handler not found for IRQ ");
        uart_put_uint(irq_id);
        uart_puts("!\r\n");
    }
    else
    {
        ctx = fnc(ctx);
    }

    gic_end_of_interrupt(irq_id);

    return ctx;
}