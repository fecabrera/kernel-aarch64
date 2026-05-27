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
struct queue wait_queue = {
    .head = NULL,
    .tail = NULL,
};

void scheduler_init()
{
    syscall_register_handler(SYSCALL_EXIT, &exit_handler);
    syscall_register_handler(SYSCALL_YIELD, &yield_handler);
    syscall_register_handler(SYSCALL_GETPID, &getpid_handler);
    syscall_register_handler(SYSCALL_WAITPID, &waitpid_handler);
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
        struct process *_proc = current->data;
        _proc->ctx = ctx;

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
    // uart_put_uint(proc->pid);
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

static void _notify_waiters(uint64_t pid, uint64_t exit_status)
{
    struct queue_entry *prev = NULL;
    struct queue_entry *curr = wait_queue.head;

    while (curr != NULL)
    {
        struct queue_entry *next = curr->next;
        struct process *waiter = curr->data;

        if (waiter->wait_pid == pid)
        {
            if (curr == wait_queue.head)
                wait_queue.head = next;

            if (curr == wait_queue.tail)
                wait_queue.tail = prev;

            if (prev != NULL)
                prev->next = next;

            waiter->state = PROC_READY;
            waiter->wait_pid = 0;
            waiter->ctx->x0 = exit_status; // return exit status to waiter

            queue_enqueue(&ready_queue, curr);

            curr = next;
        }
        else
        {
            prev = curr;
            curr = next;
        }
    }
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

    // notify waiters
    _notify_waiters(proc->pid, ctx->x1);

    // destroy process resources
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

struct cpu_context *waitpid_handler(struct cpu_context *ctx)
{
    uint64_t pid = ctx->x1;

    uart_puts("[scheduler] waitpid(");
    uart_put_uint(pid);
    uart_puts(")\r\n");

    if (current == NULL)
    {
        uart_puts("[scheduler] no current process for waitpid!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    struct process *proc = current->data;

    proc->state = PROC_BLOCKED;
    proc->wait_pid = pid;
    proc->ctx = ctx;

    queue_enqueue(&wait_queue, current);
    current = NULL;

    return scheduler_handler(ctx);
}