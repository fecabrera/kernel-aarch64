#include <queue.h>
#include <mm/heap.h>
#include <drivers/uart.h>
#include <drivers/gic.h>
#include <arch/syscall.h>
#include "scheduler.h"

struct queue_entry *current = NULL;
struct queue ready_queue = {
    .head = NULL,
    .tail = NULL,
};

void scheduler_init()
{
    syscall_register_handler(SYSCALL_EXIT, &exit_handler);
    syscall_register_handler(SYSCALL_YIELD, &yield_handler);
    syscall_register_handler(SYSCALL_GETPID, &getpid_handler);
}

int scheduler_enqueue(struct process *proc)
{
    struct queue_entry *entry = (struct queue_entry *)kmalloc(sizeof(struct queue_entry));

    if (entry == NULL)
    {
        uart_puts("[scheduler] failed to alloc entry!");
        return -1;
    }

    proc->state = PROC_READY;

    uart_puts("[scheduler] enqueue pid ");
    uart_put_uint(proc->pid);
    uart_puts("\r\n");

    entry->data = proc;

    queue_enqueue(&ready_queue, entry);
    return 0;
}

struct process *scheduler_dequeue()
{
    struct queue_entry *entry = queue_dequeue(&ready_queue);

    if (entry == NULL)
    {
        return NULL;
    }

    struct process *proc = entry->data;
    proc->state = PROC_DEAD;

    kfree(entry);

    uart_puts("[scheduler] dequeue pid ");
    uart_put_uint(proc->pid);
    uart_puts("\r\n");

    return proc;
}

struct cpu_context *scheduler_handler(struct cpu_context *ctx)
{
    if (current != NULL)
    {
        queue_enqueue(&ready_queue, current);
    }

    current = queue_dequeue(&ready_queue);
    if (current == NULL)
    {
        // uart_puts("[scheduler] no process to schedule, returning to kernel\r\n");
        return ctx;
    }

    struct process *proc = current->data;

    // uart_puts("[scheduler] context switch to pid ");
    // uart_put_uint(current->proc->pid);
    // uart_puts("\r\n");

    return proc->ctx;
}

struct cpu_context *yield_handler(struct cpu_context *ctx)
{
    uart_puts("[scheduler] yield()");

    ctx->x0 = 0;

    uart_puts(", ctx->x0 = ");
    uart_put_uint(ctx->x0);
    uart_puts("\r\n");

    return scheduler_handler(ctx);
}

struct cpu_context *exit_handler(struct cpu_context *ctx)
{
    if (current == NULL)
    {
        uart_puts("[scheduler] no current process to exit!\r\n");
        return ctx;
    }

    struct process *proc = current->data;

    uart_puts("[scheduler] exit(");
    uart_put_uint(proc->pid);
    uart_puts("), ctx->x0 = ");
    uart_put_uint(ctx->x0);
    uart_puts(", ctx->x1 = ");
    uart_put_uint(ctx->x1);
    uart_puts("\r\n");

    proc->state = PROC_DEAD;

    if (destroy_process(proc) < 0)
    {
        uart_puts("[scheduler] failed to destroy process, addr = 0x");
        uart_put_uint_hex((uintptr_t)proc);
        uart_puts("\r\n");
    }

    // free current entry
    current = NULL;

    return scheduler_handler(ctx);
}

struct cpu_context *getpid_handler(struct cpu_context *ctx)
{
    if (current == NULL)
    {
        uart_puts("[scheduler] no current process for getpid!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    struct process *proc = current->data;

    uart_puts("[scheduler] getpid(");
    uart_put_uint(proc->pid);
    uart_puts(")\r\n");

    ctx->x0 = proc->pid;
    return ctx;
}