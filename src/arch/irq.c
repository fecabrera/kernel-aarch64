#include <string.h>
#include <debug.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <drivers/pl031.h>
#include <sched/scheduler.h>
#include "cpu.h"
#include "irq.h"
#include "syscall.h"

extern void vector_table();

irq_handler_t irq_table[NUM_IRQS] = {NULL};

static void ctx_dump(struct cpu_context *ctx)
{
    printk("\r\n=== ctx dump ===\r\n");
    printk("x0  = 0x%x\r\n", ctx->x0);
    printk("x1  = 0x%x\r\n", ctx->x1);
    printk("x2  = 0x%x\r\n", ctx->x2);
    printk("x3  = 0x%x\r\n", ctx->x3);
    printk("x4  = 0x%x\r\n", ctx->x4);
    printk("x5  = 0x%x\r\n", ctx->x5);
    printk("x6  = 0x%x\r\n", ctx->x6);
    printk("x7  = 0x%x\r\n", ctx->x7);
    printk("x8  = 0x%x\r\n", ctx->x8);
    printk("x9  = 0x%x\r\n", ctx->x9);
    printk("x10 = 0x%x\r\n", ctx->x10);
    printk("x11 = 0x%x\r\n", ctx->x11);
    printk("x12 = 0x%x\r\n", ctx->x12);
    printk("x13 = 0x%x\r\n", ctx->x13);
    printk("x14 = 0x%x\r\n", ctx->x14);
    printk("x15 = 0x%x\r\n", ctx->x15);
    printk("lr  = 0x%x\r\n", ctx->lr);
    printk("elr = 0x%x\r\n", ctx->elr);
    printk("spsr= 0b");
    for (int i = sizeof(uint64_t) * 8 - 1; i >= 0; i--)
    {
        printk((ctx->spsr & (1UL << i)) ? "1" : "0");
    }
    printk("\r\n=================\r\n");
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
        dprintk("[sync] syscall(%d)\r\n", ctx->x0);
        ctx = syscall_handler(ctx);
        break;
    case ESR_EC_IABT_EL0:
        dprintk("[sync] instruction abort, elr = 0x%x, far 0x%x\r\n", elr, far);
        ctx = exit_handler(ctx);
        break;
    case ESR_EC_DABT_EL0:
        dprintk("[sync] data abort, elr = 0x%x, far = 0x%x\r\n", elr, far);
        ctx = exit_handler(ctx);
        break;
    default:
        printk("[sync] esr = 0x%x, elr = 0x%x, far = 0x%x, ec = 0x%x\r\n", esr, elr, far, ec);
        ctx_dump(ctx);

        hang();
    }

    return ctx;
}

void serr_handler()
{
    dprintk("[SError] System error!\r\n");
    hang();
}

void fiq_handler()
{
    dprintk("[FIQ]\r\n");
}

int irq_register_handler(uint32_t irq, irq_handler_t fnc)
{
    if (irq_table[irq] == NULL)
    {
        irq_table[irq] = fnc;
        dprintk("[irq] handler registered for IRQ %i, address = 0x%x\r\n", irq, fnc);
        return 0;
    }
    else
    {
        dprintk("[irq] There's already a handler registered for IRQ %i!\r\n", irq);
        return -1;
    }
}

int irq_unregister_handler(uint32_t irq)
{
    if (irq_table[irq] == NULL)
    {
        dprintk("[irq] There's no handler registered for IRQ %i!\r\n", irq);
        return -1;
    }
    else
    {
        irq_table[irq] = NULL;
        dprintk("[irq] Handler unregistered for IRQ %i!\r\n", irq);
        return 0;
    }
}

struct cpu_context *irq_handler(struct cpu_context *ctx)
{
    // Ask the GIC which interrupt fired (see below)
    uint32_t irq_id = gic_acknowledge();
    irq_handler_t fnc = irq_table[irq_id];

    if (fnc == NULL)
        dprintk("[irq] Handler not found for IRQ %i!", irq_id);
    else
        ctx = fnc(irq_id, ctx);

    gic_end_of_interrupt(irq_id);

    return ctx;
}