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
    uart_printf("\r\n=== ctx dump ===\r\n");
    uart_printf("x0  = 0x%x\r\n", ctx->x0);
    uart_printf("x1  = 0x%x\r\n", ctx->x1);
    uart_printf("x2  = 0x%x\r\n", ctx->x2);
    uart_printf("x3  = 0x%x\r\n", ctx->x3);
    uart_printf("x4  = 0x%x\r\n", ctx->x4);
    uart_printf("x5  = 0x%x\r\n", ctx->x5);
    uart_printf("x6  = 0x%x\r\n", ctx->x6);
    uart_printf("x7  = 0x%x\r\n", ctx->x7);
    uart_printf("x8  = 0x%x\r\n", ctx->x8);
    uart_printf("x9  = 0x%x\r\n", ctx->x9);
    uart_printf("x10 = 0x%x\r\n", ctx->x10);
    uart_printf("x11 = 0x%x\r\n", ctx->x11);
    uart_printf("x12 = 0x%x\r\n", ctx->x12);
    uart_printf("x13 = 0x%x\r\n", ctx->x13);
    uart_printf("x14 = 0x%x\r\n", ctx->x14);
    uart_printf("x15 = 0x%x\r\n", ctx->x15);
    uart_printf("lr  = 0x%x\r\n", ctx->lr);
    uart_printf("elr = 0x%x\r\n", ctx->elr);
    uart_printf("spsr= 0b");
    for (int i = sizeof(uint64_t) * 8 - 1; i >= 0; i--)
    {
        uart_printf((ctx->spsr & (1UL << i)) ? "1" : "0");
    }
    uart_printf("\r\n=================\r\n");
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
        uart_printf("[sync] syscall(%d)\r\n", ctx->x0);
        ctx = syscall_handler(ctx);
        break;
    case ESR_EC_IABT_EL0:
        uart_printf("[sync] instruction abort, elr = 0x%x, far 0x%x\r\n", elr, far);
        ctx = exit_handler(ctx);
        break;
    case ESR_EC_DABT_EL0:
        uart_printf("[sync] data abort, elr = 0x%x, far = 0x%x\r\n", elr, far);
        ctx = exit_handler(ctx);
        break;
    default:
        uart_printf("[sync] esr = 0x%x, elr = 0x%x, far = 0x%x, ec = 0x%x\r\n", esr, elr, far, ec);

        ctx_dump(ctx);

        hang();
    }

    return ctx;
}

void serr_handler()
{
    uart_printf("[SError] System error!\r\n");
    hang();
}

void fiq_handler()
{
    uart_printf("[FIQ]\r\n");
}

int irq_register_handler(uint32_t irq, interrupt_handler fnc)
{
    if (irq_table[irq] == NULL)
    {
        irq_table[irq] = fnc;
        uart_printf("[irq] handler registered for IRQ %i, address = 0x%x\r\n", irq, fnc);
        return 0;
    }
    else
    {
        uart_printf("[irq] There's already a handler registered for IRQ %i!\r\n", irq);
        return -1;
    }
}

int irq_unregister_handler(uint32_t irq)
{
    if (irq_table[irq] == NULL)
    {
        uart_printf("[irq] There's no handler registered for IRQ %i!\r\n", irq);
        return -1;
    }
    else
    {
        irq_table[irq] = NULL;
        uart_printf("[irq] Handler unregistered for IRQ %i!\r\n", irq);
        return 0;
    }
}

struct cpu_context *irq_handler(struct cpu_context *ctx)
{
    // Ask the GIC which interrupt fired (see below)
    uint32_t irq_id = gic_acknowledge();
    interrupt_handler fnc = irq_table[irq_id];

    if (fnc == NULL)
        uart_printf("[irq] Handler not found for IRQ %i!", irq_id);
    else
        ctx = fnc(ctx);

    gic_end_of_interrupt(irq_id);

    return ctx;
}