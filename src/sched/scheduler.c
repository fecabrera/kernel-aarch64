#include <debug.h>
#include <mm/heap.h>
#include <dsa/queue.h>
#include <dsa/deque.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <arch/syscall.h>
#include "scheduler.h"

struct cpu_context *idle_ctx;
static struct process *current = NULL;
static struct queue64 ready_queue;
static struct deque64 waitpid_queue = {
    .head = NULL,
    .tail = NULL,
};
static struct deque64 sleep_queue = {
    .head = NULL,
    .tail = NULL,
};

void scheduler_init()
{
    idle_ctx = (struct cpu_context *)kmalloc(272);

    queue64_init(&ready_queue, 10);

    syscall_register_handler(SYSCALL_EXIT, &exit_handler);
    syscall_register_handler(SYSCALL_YIELD, &yield_handler);
    syscall_register_handler(SYSCALL_GETPID, &getpid_handler);
    syscall_register_handler(SYSCALL_WAITPID, &waitpid_handler);
    syscall_register_handler(SYSCALL_FORK, &fork_handler);
    syscall_register_handler(SYSCALL_SLEEP, &sleep_handler);
    syscall_register_handler(SYSCALL_MSLEEP, &msleep_handler);
}

int scheduler_enqueue(struct process *proc)
{
    proc->state = PROC_READY;
    queue64_push(&ready_queue, (uintptr_t)proc);

    dprintk("[scheduler] enqueue(), q = { ", proc->pid);
    for (uint64_t i = 0; i < ready_queue.length; i++)
    {
        struct process *proc = (struct process *)queue64_at(&ready_queue, i);
        dprintk("%i ", proc->pid);
    }
    dprintk("}, current = %i\r\n", current ? current->pid : 0);

    return 0;
}

struct process *scheduler_dequeue()
{
    if (ready_queue.length == 0)
        return NULL;

    struct process *proc = (struct process *)queue64_pop(&ready_queue);
    proc->state = PROC_DEAD;

    dprintk("[scheduler] dequeue(%i)\r\n", proc->pid);

    return proc;
}

pid_t scheduler_spawn(proc_entry entry)
{
    dprintk("[scheduler] spawn()\r\n");

    struct process *proc = (struct process *)kmalloc(sizeof(struct process));
    create_process(proc, DEFAULT_STACK_SIZE);
    scheduler_enqueue(proc);
    process_config(proc, entry);

    return proc->pid;
}

static void _notify_sleepers()
{
    time_t uptime = timer_get_uptime();
    struct deque64_entry *entry = NULL;

    dprintk("[scheduler] __notify_sleepers(), current=%i\r\n", current ? current->pid : 0);

    while ((entry = deque64_next(&sleep_queue, entry)))
    {
        struct process *proc = (struct process *)entry->value;

        dprintk("[scheduler] pid=%i, sleep_until=%i, uptime=%i\r\n", proc->pid, proc->sleep_until, uptime);

        if (proc->sleep_until <= uptime)
        {
            dprintk("[scheduler] notifying pid %i\r\n", proc->pid);

            deque64_remove(&sleep_queue, entry);

            proc->state = PROC_READY;
            proc->sleep_until = 0;
            proc->ctx->x0 = 0;

            queue64_push(&ready_queue, (uintptr_t)proc);
        }
    }
}

struct cpu_context *scheduler_handler(struct cpu_context *ctx, time_t ms_elapsed)
{
    if (ms_elapsed > 0 && !deque64_is_empty(&sleep_queue))
        _notify_sleepers();

    struct process *previous = current;
    if (previous != NULL)
    {
        previous->ctx = ctx;
        queue64_push(&ready_queue, (uintptr_t)previous);
    }

    if (ready_queue.length == 0)
    {
        dprintk("[scheduler] idle()\r\n");
        idle_ctx->elr = (uint64_t)&halt;
        return idle_ctx;
    }

    current = (struct process *)queue64_pop(&ready_queue);

    if (previous && (previous->pid != current->pid))
    {
        printk("[scheduler] context_switch(), q = { ");

        struct process **procs = (struct process **)ready_queue.data;
        for (uint64_t i = 0; i < ready_queue.length; i++)
            printk("%i ", procs[i]->pid);

        printk("}, ");
        if (previous)
            printk("previous = %i, ", previous->pid);
        else
            printk("previous = <null>, ");
        printk("current = %i, ms_elapsed = %d ms\r\n", current->pid, ms_elapsed);
    }

    return current->ctx;
}

struct cpu_context *yield_handler(struct cpu_context *ctx)
{
    ctx->x0 = 0;

    dprintk("[scheduler] yield()\r\n");

    return scheduler_handler(ctx, 0);
}

static int _wait_pid_eq(struct deque64_entry *entry, void *pid)
{
    struct process *proc = (struct process *)entry->value;
    pid_t value = *(pid_t *)pid;

    if (proc->wait_pid == value)
        return 0;

    return -1;
}

static void _notify_waiter(struct process *proc, int64_t exit_status)
{
    dprintk("[scheduler] _notify_waiter(%i), exit_status = %i\r\n", proc->pid, exit_status);

    proc->state = PROC_READY;
    proc->wait_pid = 0;
    proc->ctx->x0 = exit_status;

    queue64_push(&ready_queue, (uintptr_t)proc);
}

static void _notify_waiters(pid_t pid, int64_t exit_status)
{
    dprintk("[scheduler] _notify_waiters(), exit_status = %i\r\n", exit_status);

    uint64_t value;
    while (deque64_find_remove(&waitpid_queue, NULL, &_wait_pid_eq, &pid, &value) == 0)
        _notify_waiter((struct process *)value, exit_status);
}

struct cpu_context *exit_handler(struct cpu_context *ctx)
{
    if (current == NULL)
    {
        dprintk("[scheduler] no current process to exit!\r\n");
        return ctx;
    }

    dprintk("[scheduler] exit(%i), ctx->x0 = %u, ctx->x1 = %u\r\n", current->pid, ctx->x0, ctx->x1);

    current->state = PROC_DEAD;

    // notify waiters
    _notify_waiters(current->pid, ctx->x1);

    // destroy process resources
    if (destroy_process(current) < 0)
    {
        dprintk("[scheduler] failed to destroy process, addr = 0x%x\r\n", current);
    }

    // clear current entry
    current = NULL;

    return scheduler_handler(ctx, 0);
}

struct cpu_context *getpid_handler(struct cpu_context *ctx)
{
    if (current == NULL)
    {
        dprintk("[scheduler] no current process for getpid!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    dprintk("[scheduler] getpid(%i)\r\n", current->pid);

    ctx->x0 = current->pid;
    return ctx;
}

struct cpu_context *waitpid_handler(struct cpu_context *ctx)
{
    pid_t pid = ctx->x1;

    dprintk("[scheduler] waitpid(%i)\r\n", pid);

    if (current == NULL)
    {
        dprintk("[scheduler] no current process for waitpid!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    current->state = PROC_BLOCKED;
    current->wait_pid = pid;
    current->ctx = ctx;

    deque64_add_right(&waitpid_queue, (uintptr_t)current);
    current = NULL;

    return scheduler_handler(ctx, 0);
}

struct cpu_context *fork_handler(struct cpu_context *ctx)
{
    if (current == NULL)
    {
        dprintk("[scheduler] no current process to fork!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    current->ctx = ctx;

    dprintk("[scheduler] fork(%i)\r\n", current->pid);

    struct process *child = (struct process *)kmalloc(sizeof(struct process));
    duplicate_process(child, current);
    scheduler_enqueue(child);

    child->ctx->x0 = 0;
    ctx->x0 = child->pid;

    return ctx;
}

struct cpu_context *sleep_handler(struct cpu_context *ctx)
{
    time_t seconds = ctx->x1;

    if (current == NULL)
    {
        dprintk("[scheduler] no current process for sleep!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    dprintk("[scheduler] sleep(%i)\r\n", seconds);

    time_t sleep_duration = seconds * 1000;
    current->state = PROC_BLOCKED;
    current->sleep_until = timer_get_uptime() + sleep_duration;
    current->ctx = ctx;

    deque64_add_right(&sleep_queue, (uintptr_t)current);
    current = NULL;

    return scheduler_handler(ctx, 0);
}

struct cpu_context *msleep_handler(struct cpu_context *ctx)
{
    mseconds_t mseconds = ctx->x1;

    if (current == NULL)
    {
        dprintk("[scheduler] no current process for msleep!\r\n");
        ctx->x0 = -1;
        return ctx;
    }

    dprintk("[scheduler] msleep(%i)\r\n", mseconds);

    current->state = PROC_BLOCKED;
    current->sleep_until = timer_get_uptime() + mseconds;
    current->ctx = ctx;

    deque64_add_right(&sleep_queue, (uintptr_t)current);
    current = NULL;

    return scheduler_handler(ctx, 0);
}
