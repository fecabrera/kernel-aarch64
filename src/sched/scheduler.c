#include <mm/heap.h>
#include <drivers/uart.h>
#include <drivers/gic.h>
#include <arch/syscall.h>
#include "scheduler.h"

struct scheduler_entry *head = NULL;
struct scheduler_entry *tail = NULL;

void scheduler_init()
{
    irq_register_handler(GICD_SGIR_IRQ_0, &yield_handler);
    syscall_register_handler(SYSCALL_EXIT, &exit_handler);
    syscall_register_handler(SYSCALL_YIELD, &yield_handler);
}

static struct scheduler_entry *_enqueue_entry(struct scheduler_entry *entry)
{
    entry->next = NULL;

    if (head == NULL)
    {
        head = tail = entry;
    }
    else
    {
        tail->next = entry;
        tail = entry;
    }

    return entry;
}

static struct scheduler_entry *_dequeue_entry()
{
    if (head == NULL)
    {
        return NULL;
    }

    struct scheduler_entry *entry = head;
    head = entry->next;

    if (head == NULL)
    {
        tail = NULL;
    }

    return entry;
}

static struct scheduler_entry *_cycle_entry()
{
    struct scheduler_entry *entry = _dequeue_entry();

    if (entry == NULL)
    {
        return NULL;
    }

    _enqueue_entry(entry);

    return entry;
};

struct scheduler_entry *scheduler_enqueue(struct process *proc)
{
    struct scheduler_entry *entry = (struct scheduler_entry *)kmalloc(sizeof(struct scheduler_entry));

    if (entry == NULL)
    {
        uart_puts("[scheduler] failed to alloc entry!");
        return NULL;
    }

    entry->proc = proc;
    entry->proc->state = PROC_READY;

    uart_puts("[scheduler] enqueue pid ");
    uart_put_uint(entry->proc->pid);
    uart_puts("\r\n");

    return _enqueue_entry(entry);
}

struct process *scheduler_dequeue()
{
    struct scheduler_entry *entry = _dequeue_entry();

    if (entry == NULL)
    {
        return NULL;
    }

    struct process *proc = entry->proc;
    proc->state = PROC_DEAD;

    kfree(entry);

    uart_puts("[scheduler] dequeue pid ");
    uart_put_uint(proc->pid);
    uart_puts("\r\n");

    return proc;
}

struct cpu_context *scheduler_handler(struct cpu_context *ctx)
{
    struct scheduler_entry *entry = _cycle_entry();

    if (entry == NULL)
    {
        // if there are no processes, return the current ctx
        return ctx;
    }

    // return ctx
    return entry->proc->ctx;
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
    uart_puts("[scheduler] exit(), ctx->x0 = ");
    uart_put_uint(ctx->x0);
    uart_puts("\r\n");

    struct process *proc = scheduler_dequeue();
    proc->state = PROC_DEAD;

    destroy_process(proc);

    return scheduler_handler(ctx);
}