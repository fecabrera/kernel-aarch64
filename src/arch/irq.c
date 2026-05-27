#include <string.h>
#include <drivers/uart.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <drivers/rtc.h>
#include <sched/scheduler.h>
#include "cpu.h"
#include "irq.h"
#include "syscall.h"

extern void vector_table();

interrupt_handler irq_table[NUM_IRQS] = {NULL};

static void ctx_dump(struct cpu_context *ctx)
{
    uart_puts("\r\n=== ctx dump ===\r\n");
    uart_puts("x0  = 0x");
    uart_put_uint_hex(ctx->x0);
    uart_puts("\r\nx1  = 0x");
    uart_put_uint_hex(ctx->x1);
    uart_puts("\r\nx2  = 0x");
    uart_put_uint_hex(ctx->x2);
    uart_puts("\r\nx3  = 0x");
    uart_put_uint_hex(ctx->x3);
    uart_puts("\r\nx4  = 0x");
    uart_put_uint_hex(ctx->x4);
    uart_puts("\r\nx5  = 0x");
    uart_put_uint_hex(ctx->x5);
    uart_puts("\r\nx6  = 0x");
    uart_put_uint_hex(ctx->x6);
    uart_puts("\r\nx7  = 0x");
    uart_put_uint_hex(ctx->x7);
    uart_puts("\r\nx8  = 0x");
    uart_put_uint_hex(ctx->x8);
    uart_puts("\r\nx9  = 0x");
    uart_put_uint_hex(ctx->x9);
    uart_puts("\r\nx10 = 0x");
    uart_put_uint_hex(ctx->x10);
    uart_puts("\r\nx11 = 0x");
    uart_put_uint_hex(ctx->x11);
    uart_puts("\r\nx12 = 0x");
    uart_put_uint_hex(ctx->x12);
    uart_puts("\r\nx13 = 0x");
    uart_put_uint_hex(ctx->x13);
    uart_puts("\r\nx14 = 0x");
    uart_put_uint_hex(ctx->x14);
    uart_puts("\r\nx15 = 0x");
    uart_put_uint_hex(ctx->x15);
    uart_puts("\r\nlr  = 0x");
    uart_put_uint_hex(ctx->lr);
    uart_puts("\relr = 0x");
    uart_put_uint_hex(ctx->elr);
    uart_puts("\r\nspsr= 0b");
    for (int i = sizeof(uint64_t) * 8 - 1; i >= 0; i--)
    {
        uart_puts((ctx->spsr & (1UL << i)) ? "1" : "0");
    }
    uart_puts("\r\n=================\r\n");
}

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
        uart_puts("[sync] syscall, ctx->x0 = ");
        uart_put_uint(ctx->x0);
        uart_puts("\r\n");
        ctx = syscall_handler(ctx);
        break;
    case ESR_EC_IABT_EL0:
        uart_puts("[sync] instruction abort, elr = 0x");
        uart_put_uint_hex(elr);
        uart_puts(", far = 0x");
        uart_put_uint_hex(far);
        uart_puts("\r\n");
        ctx = exit_handler(ctx);
        break;
    case ESR_EC_DABT_EL0:
        uart_puts("[sync] data abort, elr = 0x");
        uart_put_uint_hex(elr);
        uart_puts(", far = 0x");
        uart_put_uint_hex(far);
        uart_puts("\r\n");
        ctx = exit_handler(ctx);
        break;
    default:
        uart_puts("[sync] esr = 0x");
        uart_put_uint_hex(esr);
        uart_puts(", elr = 0x");
        uart_put_uint_hex(elr);
        uart_puts(", far = 0x");
        uart_put_uint_hex(far);
        uart_puts(", ec = 0x");
        uart_put_uint_hex(ec);
        uart_puts("\r\n");

        ctx_dump(ctx);

        hang();
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

int irq_register_handler(uint32_t irq, interrupt_handler fnc)
{
    if (irq_table[irq] == NULL)
    {
        irq_table[irq] = fnc;

        uart_puts("[irq] handler registered for IRQ ");
        uart_put_uint(irq);
        uart_puts(", address 0x");
        uart_put_uint_hex((uintptr_t)fnc);
        uart_puts("\r\n");

        return 0;
    }
    else
    {
        uart_puts("[irq] There's already a handler registered for IRQ ");
        uart_put_uint(irq);
        uart_puts("!\r\n");

        return -1;
    }
}

int irq_unregister_handler(uint32_t irq)
{
    if (irq_table[irq] == NULL)
    {
        uart_puts("[irq] There's no handler registered for IRQ ");
        uart_put_uint(irq);
        uart_puts("!\r\n");

        return -1;
    }
    else
    {
        irq_table[irq] = NULL;

        uart_puts("[irq] Handler unregistered for IRQ ");
        uart_put_uint(irq);
        uart_puts("!\r\n");

        return 0;
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