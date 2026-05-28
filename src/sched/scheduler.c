#include <mm/heap.h>
#include <dsa/queue.h>
#include <dsa/deque.h>
#include <drivers/uart.h>
#include <drivers/gic.h>
#include <arch/syscall.h>
#include "scheduler.h"

static struct process *current = NULL;
static struct queue64 ready_queue;
static struct deque64 wait_queue = {
    .head = NULL,
    .tail = NULL,
};

void scheduler_init()
{
    queue64_init(&ready_queue, 10);

    syscall_register_handler(SYSCALL_EXIT, &exit_handler);
    syscall_register_handler(SYSCALL_YIELD, &yield_handler);
    syscall_register_handler(SYSCALL_GETPID, &getpid_handler);
    syscall_register_handler(SYSCALL_WAITPID, &waitpid_handler);
    syscall_register_handler(SYSCALL_FORK, &fork_handler);
}

int scheduler_enqueue(struct process *proc)
{
    proc->state = PROC_READY;
    queue64_push(&ready_queue, (uintptr_t)proc);

    uart_puts("[scheduler] enqueue(");
    uart_put_uint(proc->pid);
    uart_puts("), q = { ");
    for (uint64_t i = 0; i < ready_queue.length; i++)
    {
        struct process *proc = (struct process *)queue64_at(&ready_queue, i);
        uart_put_uint(proc->pid);
        uart_puts(" ");
    }
    uart_puts("}, current = ");
    uart_put_uint(current->pid);
    uart_puts("\r\n");

    return 0;
}

struct process *scheduler_dequeue()
{
    if (ready_queue.length == 0)
    {
        return NULL;
    }

    struct process *proc = (struct process *)queue64_pop(&ready_queue);
    proc->state = PROC_DEAD;

    uart_puts("[scheduler] dequeue(");
    uart_put_uint(proc->pid);
    uart_puts(")\r\n");

    return proc;
}

struct cpu_context *scheduler_handler(struct cpu_context *ctx)
{
    if (current != NULL)
    {
        current->ctx = ctx;
        queue64_push(&ready_queue, (uintptr_t)current);
    }

    if (ready_queue.length == 0)
    {
        return ctx;
    }

    current = (struct process *)queue64_pop(&ready_queue);

    if (current->ctx != ctx)
    {
        uart_puts("[scheduler] context_switch(), q = { ");
        struct process **procs = (struct process **)ready_queue.data;
        for (uint64_t i = 0; i < ready_queue.length; i++)
        {
            uart_put_uint(procs[i]->pid);
            uart_puts(" ");
        }
        uart_puts("}, current = ");
        uart_put_uint(current->pid);
        uart_puts("\r\n");
    }

    return current->ctx;
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

static int _pid_eq(struct deque64_entry *entry, void *pid)
{
    struct process *proc = (struct process *)entry->value;
    uint64_t value = *(uint64_t *)pid;

    if (proc->wait_pid == value)
        return 0;

    return -1;
}

static void _notify_waiter(struct process *proc, uint64_t exit_status)
{
    uart_puts("[scheduler] _notify_waiter(");
    uart_put_uint(proc->pid);
    uart_puts("), exit_status = ");
    uart_put_uint(exit_status);
    uart_puts("\r\n");

    proc->state = PROC_READY;
    proc->wait_pid = 0;
    proc->ctx->x0 = exit_status;

    queue64_push(&ready_queue, (uintptr_t)proc);
}

static void _notify_waiters(int64_t pid, uint64_t exit_status)
{
    struct deque64_entry *entry = NULL;
    uart_puts("[scheduler] _notify_waiters(), exit_status = ");
    uart_put_uint(exit_status);
    uart_puts("\r\n");

    while ((entry = deque64_find_remove(&wait_queue, entry, &_pid_eq, &pid)))
    {
        _notify_waiter((struct process *)entry->value, exit_status);

        struct deque64_entry *next = entry->next;
        kfree(entry);
        entry = next;
    }
}

struct cpu_context *exit_handler(struct cpu_context *ctx)
{
    if (current == NULL)
    {
        uart_puts("[scheduler] no current process to exit!\r\n");
        return ctx;
    }

    uart_puts("[scheduler] exit(");
    uart_put_uint(current->pid);
    uart_puts("), ctx->x0 = ");
    uart_put_uint(ctx->x0);
    uart_puts(", ctx->x1 = ");
    uart_put_uint(ctx->x1);
    uart_puts("\r\n");

    current->state = PROC_DEAD;

    // notify waiters
    _notify_waiters(current->pid, ctx->x1);

    // destroy process resources
    if (destroy_process(current) < 0)
    {
        uart_puts("[scheduler] failed to destroy process, addr = 0x");
        uart_put_uint_hex((uintptr_t)current);
        uart_puts("\r\n");
    }

    // clear current entry
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

    uart_puts("[scheduler] getpid(");
    uart_put_uint(current->pid);
    uart_puts(")\r\n");

    ctx->x0 = current->pid;
    return ctx;
}

struct cpu_context *waitpid_handler(struct cpu_context *ctx)
{
    int64_t pid = ctx->x1;

    uart_puts("[scheduler] waitpid(");
    uart_put_uint(pid);
    uart_puts(")\r\n");

    if (current == NULL)
    {
        uart_puts("[scheduler] no current process for waitpid!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    current->state = PROC_BLOCKED;
    current->wait_pid = pid;
    current->ctx = ctx;

    deque64_add_right(&wait_queue, (uintptr_t)current);
    current = NULL;

    return scheduler_handler(ctx);
}

struct cpu_context *fork_handler(struct cpu_context *ctx)
{
    if (current == NULL)
    {
        uart_puts("[scheduler] no current process for fork!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    current->ctx = ctx;

    uart_puts("[scheduler] fork(");
    uart_put_uint(current->pid);
    uart_puts(")\r\n");

    struct process *child = (struct process *)kmalloc(sizeof(struct process));
    duplicate_process(child, current);
    scheduler_enqueue(child);

    child->ctx->x0 = 0;
    ctx->x0 = child->pid;

    return ctx;
}