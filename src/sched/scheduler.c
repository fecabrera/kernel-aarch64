#include "scheduler.h"
#include "fs/vfs.h"
#include <arch/syscall.h>
#include <debug.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <dsa/queue.h>
#include <dsa/set.h>
#include <mm/heap.h>

static struct cpu_context *idle_ctx;
static struct process *current = NULL;
static struct queue64 ready_queue;
static struct set64 waitpid_queue;
static struct set64 sleep_queue;

void scheduler_init() {
    idle_ctx = (struct cpu_context *)kmalloc(272);

    queue64_init(&ready_queue, 10);
    set64_init(&waitpid_queue, 10);
    set64_init(&sleep_queue, 10);

    syscall_register_handler(SYSCALL_EXIT, &exit_handler);
    syscall_register_handler(SYSCALL_YIELD, &yield_handler);
    syscall_register_handler(SYSCALL_GETPID, &getpid_handler);
    syscall_register_handler(SYSCALL_WAITPID, &waitpid_handler);
    syscall_register_handler(SYSCALL_FORK, &fork_handler);
    syscall_register_handler(SYSCALL_SLEEP, &sleep_handler);
    syscall_register_handler(SYSCALL_MSLEEP, &msleep_handler);
}

int scheduler_enqueue(struct process *proc) {
    proc->state = PROC_READY;
    queue64_push(&ready_queue, (uintptr_t)proc);

    dprintk("[scheduler] enqueue(), q = { ", proc->pid);
    for (uint64_t i = 0; i < ready_queue.length; i++) {
        struct process *proc = (struct process *)queue64_at(&ready_queue, i);
        dprintk("%i ", proc->pid);
    }
    dprintk("}, current = %i\n", current ? current->pid : 0);

    return 0;
}

struct process *scheduler_dequeue() {
    if (ready_queue.length == 0)
        return NULL;

    struct process *proc = (struct process *)queue64_pop(&ready_queue);
    proc->state = PROC_DEAD;

    dprintk("[scheduler] dequeue(%i)\n", proc->pid);

    return proc;
}

pid_t scheduler_spawn(proc_entry entry) {
    dprintk("[scheduler] spawn()\n");

    struct process *proc = (struct process *)kmalloc(sizeof(struct process));

    dprintk("[scheduler] creating process\n");
    create_process(proc, DEFAULT_STACK_SIZE);

    dprintk("[scheduler] configuring process\n");
    process_config(proc, entry);

    dprintk("[scheduler] enqueueing process\n");
    scheduler_enqueue(proc);

    return proc->pid;
}

static void _notify_sleepers() {
    time_t uptime = timer_get_uptime();

    dprintk("[scheduler] __notify_sleepers(), current=%i\n", current ? current->pid : 0);

    for (size_t i = 0; i < sleep_queue.capacity; i++) {
        struct set64_entry *entry = (struct set64_entry *)&sleep_queue.entries[i];
        if (entry->state != SET64_SLOT_OCCUPIED)
            continue;

        struct process *proc = (struct process *)entry->value;

        dprintk("[scheduler] pid=%i, sleep_until=%i, uptime=%i\n", proc->pid, proc->sleep_until,
                uptime);

        if (proc->sleep_until <= uptime) {
            dprintk("[scheduler] notifying pid %i\n", proc->pid);

            set64_remove(&sleep_queue, proc->pid);

            proc->state = PROC_READY;
            proc->sleep_until = 0;
            proc->ctx->x0 = 0;

            queue64_push(&ready_queue, (uintptr_t)proc);
        }
    }
}

struct cpu_context *scheduler_handler(struct cpu_context *ctx, time_t ms_elapsed) {
    if (ms_elapsed > 0 && sleep_queue.length > 0)
        _notify_sleepers();

    struct process *previous = current;
    if (previous != NULL) {
        previous->ctx = ctx;
        queue64_push(&ready_queue, (uintptr_t)previous);
    }

    if (ready_queue.length == 0) {
        dprintk("[scheduler] idle()\n");
        idle_ctx->elr = (uint64_t)&halt;
        return idle_ctx;
    }

    current = (struct process *)queue64_pop(&ready_queue);

    if (previous && (previous->pid != current->pid)) {
        dprintk("[scheduler] context_switch(), q = { ");

        struct process **procs = (struct process **)ready_queue.data;
        for (uint64_t i = 0; i < ready_queue.length; i++)
            dprintk("%i ", procs[i]->pid);

        dprintk("}, ");
        if (previous)
            dprintk("previous = %i, ", previous->pid);
        else
            dprintk("previous = <null>, ");
        dprintk("current = %i, ms_elapsed = %d ms\n", current->pid, ms_elapsed);
    }

    return current->ctx;
}

struct cpu_context *yield_handler(struct cpu_context *ctx) {
    ctx->x0 = 0;

    dprintk("[scheduler] yield()\n");

    return scheduler_handler(ctx, 0);
}

static void _notify_waiter(struct process *proc, int64_t exit_status) {
    dprintk("[scheduler] _notify_waiter(%i), exit_status=%i\n", proc->pid, exit_status);

    proc->state = PROC_READY;
    proc->wait_pid = 0;
    proc->ctx->x0 = exit_status;

    queue64_push(&ready_queue, (uintptr_t)proc);
}

static void _notify_waiters(pid_t wait_pid, int64_t exit_status) {
    dprintk("[scheduler] _notify_waiters(%d), exit_status=%i, waitpid_queue->length=%d\n", wait_pid,
            exit_status, waitpid_queue.length);

    for (size_t i = 0; i < waitpid_queue.capacity; i++) {
        dprintk("[scheduler] i=%d, waitpid_queue->capacity=%d\n", i, waitpid_queue.capacity);

        struct set64_entry *entry = waitpid_queue.entries + i;
        dprintk("entry->key=%d, entry->value=%d, entry->state=%d\n", entry->key, entry->value,
                entry->state);
        if (entry->state != SET64_SLOT_OCCUPIED)
            continue;

        struct process *proc = (struct process *)entry->value;
        dprintk("proc->pid=%i, proc->wait_pid=%i\n", proc->pid, proc->wait_pid);

        if (proc->wait_pid != wait_pid)
            continue;

        set64_remove(&waitpid_queue, proc->pid);
        _notify_waiter(proc, exit_status);
    }
}

struct cpu_context *exit_handler(struct cpu_context *ctx) {
    if (current == NULL) {
        dprintk("[scheduler] no current process to exit!\n");
        return ctx;
    }

    dprintk("[scheduler] exit(%i), ctx->x0 = %u, ctx->x1 = %u\n", current->pid, ctx->x0, ctx->x1);

    current->state = PROC_DEAD;

    // notify waiters
    _notify_waiters(current->pid, ctx->x1);

    // destroy process resources
    if (destroy_process(current) < 0) {
        dprintk("[scheduler] failed to destroy process, addr = 0x%x\n", current);
    }

    // clear current entry
    current = NULL;

    return scheduler_handler(ctx, 0);
}

struct cpu_context *getpid_handler(struct cpu_context *ctx) {
    if (current == NULL) {
        dprintk("[scheduler] no current process for getpid!\n");
        ctx->x0 = -1;
        return ctx;
    }

    dprintk("[scheduler] getpid(%i)\n", current->pid);

    ctx->x0 = current->pid;
    return ctx;
}

struct cpu_context *waitpid_handler(struct cpu_context *ctx) {
    pid_t pid = ctx->x1;

    dprintk("[scheduler] waitpid(%i)\n", pid);

    if (current == NULL) {
        dprintk("[scheduler] no current process for waitpid!\n");
        ctx->x0 = -1;
        return ctx;
    }

    current->state = PROC_BLOCKED;
    current->wait_pid = pid;
    current->ctx = ctx;

    set64_set(&waitpid_queue, current->pid, (uintptr_t)current);
    current = NULL;

    return scheduler_handler(ctx, 0);
}

struct cpu_context *fork_handler(struct cpu_context *ctx) {
    if (current == NULL) {
        dprintk("[scheduler] no current process to fork!\n");
        ctx->x0 = -1;
        return ctx;
    }

    current->ctx = ctx;

    dprintk("[scheduler] fork(%i)\n", current->pid);

    struct process *child = (struct process *)kmalloc(sizeof(struct process));
    duplicate_process(child, current);
    scheduler_enqueue(child);

    child->ctx->x0 = 0;
    ctx->x0 = child->pid;

    return ctx;
}

struct cpu_context *sleep_handler(struct cpu_context *ctx) {
    time_t seconds = ctx->x1;

    if (current == NULL) {
        dprintk("[scheduler] no current process for sleep!\n");
        ctx->x0 = -1;
        return ctx;
    }

    dprintk("[scheduler] sleep(%i)\n", seconds);

    time_t sleep_duration = seconds * 1000;
    current->state = PROC_BLOCKED;
    current->sleep_until = timer_get_uptime() + sleep_duration;
    current->ctx = ctx;

    set64_set(&sleep_queue, current->pid, (uintptr_t)current);
    current = NULL;

    return scheduler_handler(ctx, 0);
}

struct cpu_context *msleep_handler(struct cpu_context *ctx) {
    mseconds_t mseconds = ctx->x1;

    if (current == NULL) {
        dprintk("[scheduler] no current process for msleep!\n");
        ctx->x0 = -1;
        return ctx;
    }

    dprintk("[scheduler] msleep(%i)\n", mseconds);

    current->state = PROC_BLOCKED;
    current->sleep_until = timer_get_uptime() + mseconds;
    current->ctx = ctx;

    set64_set(&sleep_queue, current->pid, (uintptr_t)current);
    current = NULL;

    return scheduler_handler(ctx, 0);
}
