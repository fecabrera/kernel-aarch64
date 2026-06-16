#include "irq.h"
#include "cpu.h"
#include "syscall.h"
#include <debug.h>
#include <drivers/gic.h>
#include <drivers/pl031.h>
#include <drivers/timer.h>
#include <dsa/set.h>
#include <sched/scheduler.h>
#include <string.h>

extern void vector_table();

static struct set64 _irq_table;

static void ctx_dump(struct cpu_context *ctx) {
    printk("\n=== ctx dump ===\n");
    printk("x0  = 0x%x\n", ctx->x0);
    printk("x1  = 0x%x\n", ctx->x1);
    printk("x2  = 0x%x\n", ctx->x2);
    printk("x3  = 0x%x\n", ctx->x3);
    printk("x4  = 0x%x\n", ctx->x4);
    printk("x5  = 0x%x\n", ctx->x5);
    printk("x6  = 0x%x\n", ctx->x6);
    printk("x7  = 0x%x\n", ctx->x7);
    printk("x8  = 0x%x\n", ctx->x8);
    printk("x9  = 0x%x\n", ctx->x9);
    printk("x10 = 0x%x\n", ctx->x10);
    printk("x11 = 0x%x\n", ctx->x11);
    printk("x12 = 0x%x\n", ctx->x12);
    printk("x13 = 0x%x\n", ctx->x13);
    printk("x14 = 0x%x\n", ctx->x14);
    printk("x15 = 0x%x\n", ctx->x15);
    printk("lr  = 0x%x\n", ctx->lr);
    printk("elr = 0x%x\n", ctx->elr);
    printk("spsr= 0b");
    for (int i = sizeof(uint64_t) * 8 - 1; i >= 0; i--) {
        printk((ctx->spsr & (1UL << i)) ? "1" : "0");
    }
    printk("\n=================\n");
}

// ── Install vector table ────────────────────────────

void irq_init() {
    // Point CPU to our vector table
    __asm__ volatile("msr vbar_el1, %0\n"
                     "isb\n" // Instruction sync barrier
                     ::"r"(vector_table));

    set64_init(&_irq_table, 10);
}

// ── ESR_EL1 — decode what caused a sync exception ──

struct cpu_context *sync_handler(struct cpu_context *ctx, uint64_t esr, uint64_t elr,
                                 uint64_t far) {
    uint32_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

    switch (ec) {
    case ESR_EC_SVC64:
        dprintk("[sync] syscall(%d)\n", ctx->x0);
        ctx = syscall_handler(ctx);
        break;
    case ESR_EC_IABT_EL0:
        dprintk("[sync] instruction abort, elr = 0x%x, far 0x%x\n", elr, far);
        ctx = exit_handler(ctx);
        break;
    case ESR_EC_DABT_EL0:
        dprintk("[sync] data abort, elr = 0x%x, far = 0x%x\n", elr, far);
        ctx = exit_handler(ctx);
        break;
    default:
        printk("[sync] esr = 0x%x, elr = 0x%x, far = 0x%x, ec = 0x%x\n", esr, elr, far, ec);
        ctx_dump(ctx);

        hang();
    }

    return ctx;
}

void serr_handler() {
    dprintk("[SError] System error!\n");
    hang();
}

void fiq_handler() { dprintk("[FIQ]\n"); }

static irq_handler_t _get_irq_handler(uint64_t irq) {
    uintptr_t irq_handler_ptr;
    if (set64_get(&_irq_table, irq, &irq_handler_ptr) == 1)
        return (irq_handler_t)irq_handler_ptr;
    else
        return NULL;
}

static void _set_irq_handler(uint64_t irq, irq_handler_t fnc) {
    set64_set(&_irq_table, irq, (uintptr_t)fnc);
}

static void _remove_irq_handler(uint64_t irq) { set64_remove(&_irq_table, irq); }

int irq_register_handler(uint32_t irq, irq_handler_t fnc) {
    if (_get_irq_handler(irq) == NULL) {
        _set_irq_handler(irq, fnc);
        dprintk("[irq] handler registered for IRQ %i, address = 0x%x\n", irq, fnc);
        return 0;
    } else {
        dprintk("[irq] There's already a handler registered for IRQ %i!\n", irq);
        return -1;
    }
}

int irq_unregister_handler(uint32_t irq) {
    if (_get_irq_handler(irq) == NULL) {
        dprintk("[irq] There's no handler registered for IRQ %i!\n", irq);
        return -1;
    } else {
        _remove_irq_handler(irq);
        dprintk("[irq] Handler unregistered for IRQ %i!\n", irq);
        return 0;
    }
}

struct cpu_context *irq_handler(struct cpu_context *ctx) {
    // Ask the GIC which interrupt fired (see below)
    uint32_t irq_id = gic_acknowledge();
    irq_handler_t fnc = _get_irq_handler(irq_id);

    if (fnc == NULL)
        dprintk("[irq] Handler not found for IRQ %i!\n", irq_id);
    else
        ctx = fnc(irq_id, ctx);

    gic_end_of_interrupt(irq_id);

    return ctx;
}