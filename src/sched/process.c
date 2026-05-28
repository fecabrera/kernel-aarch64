#include <mm/heap.h>
#include <drivers/uart.h>
#include <arch/cpu.h>
#include "process.h"

uint64_t next_pid = 1;

int create_process(struct process *proc, size_t stack_size)
{
    uint8_t *stack = (uint8_t *)kmalloc_aligned(stack_size, 16);

    if (stack == NULL)
    {
        return -1;
    }

    // vectors.S save_context uses a 272-byte frame (sizeof(cpu_context)=264 + 8
    // bytes padding for 16-byte SP alignment). Place ctx 272 bytes from the top
    // so that after restore_context's "add sp, sp, #272", sp == stack + stack_size
    // (16-byte aligned).
    struct cpu_context *ctx = (struct cpu_context *)(stack + stack_size - 272);
    memset(ctx, 0, sizeof(struct cpu_context));

    proc->pid = next_pid++;
    proc->state = PROC_CREATED;
    proc->ctx = ctx;
    proc->stack = stack;
    proc->stack_size = stack_size;

    return 0;
}

int duplicate_process(struct process *dest, struct process *src)
{
    size_t stack_size = src->stack_size;
    uint8_t *stack = (uint8_t *)kmalloc_aligned(src->stack_size, 16);

    if (stack == NULL)
    {
        return -1;
    }

    uintptr_t ctx_offset = (uintptr_t)src->ctx - (uintptr_t)src->stack;
    struct cpu_context *ctx = (struct cpu_context *)(stack + ctx_offset);
    memcpy(ctx, src->ctx, sizeof(struct cpu_context));

    dest->pid = next_pid++;
    dest->state = PROC_CREATED;
    dest->ctx = ctx;
    dest->stack = stack;
    dest->stack_size = stack_size;

    uart_puts("stack = 0x");
    uart_put_uint_hex((uintptr_t)src->stack);
    uart_puts(", ctx = 0x");
    uart_put_uint_hex((uintptr_t)src->ctx);
    uart_puts(", offset = 0x");
    uart_put_uint_hex((uintptr_t)src->ctx - (uintptr_t)src->stack);
    uart_puts("\r\n");

    uart_puts("stack = 0x");
    uart_put_uint_hex((uintptr_t)dest->stack);
    uart_puts(", ctx = 0x");
    uart_put_uint_hex((uintptr_t)dest->ctx);
    uart_puts(", offset = 0x");
    uart_put_uint_hex((uintptr_t)dest->ctx - (uintptr_t)dest->stack);
    uart_puts("\r\n");

    return 0;
}

void process_config(struct process *proc, proc_entry entry)
{
    struct cpu_context *ctx = proc->ctx;
    ctx->elr = (uint64_t)entry;
    ctx->spsr = SPSR_EL1h;
}

int destroy_process(struct process *proc)
{
    if (proc->state != PROC_DEAD)
    {
        return -1;
    }

    kfree(proc->stack);

    proc->stack = NULL;
    proc->ctx = NULL;

    return 0;
}