#include <mm/heap.h>
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

    struct cpu_context *ctx = (struct cpu_context *)(stack + stack_size - sizeof(struct cpu_context));
    memset(ctx, 0, sizeof(struct cpu_context));

    proc->pid = next_pid++;
    proc->state = PROC_CREATED;
    proc->ctx = ctx;
    proc->stack = stack;
    proc->stack_size = stack_size;

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