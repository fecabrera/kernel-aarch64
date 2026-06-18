import "debug";
import "cpu";
import "process";
import "queue";
import "set";

@extern let idle_ctx: struct cpu_context*;
@extern let current: struct process*;

@static let ready_queue: struct queue<struct process*>;
@static let waitpid_queue: struct set<int64, struct process*>;
@static let sleep_queue: struct set<int64, struct process*>;

/**
 * Registers all scheduler syscall handlers. Must be called before irq_enable().
 *
 * Registered handlers:
 *   SYSCALL_EXIT             → syscall_exit_handler
 *   SYSCALL_YIELD            → syscall_yield_handler
 *   SYSCALL_GETPID           → syscall_getpid_handler
 *   SYSCALL_WAITPID          → syscall_waitpid_handler
 *   SYSCALL_FORK             → syscall_fork_handler
 *   SYSCALL_SLEEP            → syscall_sleep_handler
 *   SYSCALL_MSLEEP           → syscall_msleep_handler
 */
@extern fn scheduler_init();

/**
 * Sets proc->state to PROC_READY and pushes it onto the tail of the ready queue.
 *
 * @param proc: process to enqueue; must be fully initialized (create_process
 *              and process_config called)
 *
 * @return 0 on success
 */
@extern fn scheduler_enqueue(proc: struct process*) -> int32;

/**
 * Returns the process currently scheduled on this CPU, or null when none is
 * running (e.g. while the idle context executes). Backed by the C-side
 * `current` global.
 *
 * @return pointer to the running process, or null
 */
fn scheduler_get_current_process() -> struct process* {
    return current;
}

/**
 * Sets the process considered currently scheduled. Pass null to clear it (done
 * by the scheduler when a process blocks or exits). Backed by the C-side
 * `current` global.
 *
 * @param proc: process to mark as running, or null to clear
 */
fn scheduler_set_current_process(proc: struct process*) {
    current = proc;
}

/**
 * Removes the head of the ready queue, marks the process PROC_DEAD, and
 * returns it. Does not affect `current`.
 *
 * @return pointer to the dequeued process, or NULL if the queue is empty
 */
@extern fn scheduler_dequeue() -> struct process*;

/**
 * Allocates a process, configures it with the given entry point, and enqueues
 * it on the ready queue. Convenience wrapper around create_process,
 * process_config, and scheduler_enqueue.
 *
 * @param entry: function to run as the process entry point
 *
 * @return PID of the newly spawned process
 */
fn scheduler_spawn(entry: fn ()) -> int64 {
    dprintk("[scheduler] spawn()\n");

    let proc: struct process* = alloc<struct process>(1);

    dprintk("[scheduler] creating process\n");
    create_process(proc, DEFAULT_STACK_SIZE);

    dprintk("[scheduler] configuring process\n");
    process_config(proc, entry);

    dprintk("[scheduler] enqueueing process\n");
    scheduler_enqueue(proc);

    return proc->pid;
}

/**
 * FIFO scheduler. Ticks the sleep queue by ms_elapsed, waking any processes
 * whose sleep has expired. Then saves ctx into current->ctx and re-enqueues
 * current (if non-NULL), and pops the head of the ready queue as the new
 * current. Called by the timer IRQ handler on each tick and by
 * yield/exit/waitpid/fork/sleep handlers.
 *
 * @param ctx:        saved context of the interrupted task; returned unchanged
 *                    if the ready queue is empty
 * @param ms_elapsed: milliseconds elapsed since the last tick; passed to the
 *                    sleep queue to decrement sleep_for counters
 *
 * @return saved context of the next task to run
 */
@extern fn scheduler_handler(ctx: struct cpu_context*, ms_elapsed: uint64) -> struct cpu_context*;

/**
 * Wakes every process blocked in waitpid on wait_pid, delivering exit_status as
 * each waiter's return value and re-enqueueing it on the ready queue. Bound to
 * the C scheduler's internal `_notify_waiters`; called by syscall_exit_handler.
 *
 * @param wait_pid:    PID of the process that exited
 * @param exit_status: status to deliver to the waiters
 */
@extern @symbol("_notify_waiters") fn notify_waiters(wait_pid: int64, exit_status: int64);

/**
 * Syscall handler for SYSCALL_EXIT. Marks current PROC_DEAD, notifies any
 * waiters (passing ctx->x1 as exit status), destroys the process, and calls
 * scheduler_handler to run the next task. Returns ctx unchanged if no process
 * is currently running.
 *
 * @param ctx: saved context of the exiting process (ctx->x1 holds the exit status)
 *
 * @return saved context of the next task to run
 */
fn syscall_exit_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process to exit!\n");
        return ctx;
    }

    dprintk("[scheduler] exit(%i), ctx->x0 = %u, ctx->x1 = %u\n", proc->pid, ctx->x[0], ctx->x[1]);

    proc->state = PROC_DEAD;

    // notify waiters
    notify_waiters(proc->pid, ctx->x[1] as int64);

    // destroy process resources
    if (destroy_process(proc) < 0) {
        dprintk("[scheduler] failed to destroy process, addr = %p\n", proc);
    }

    // clear current entry
    scheduler_set_current_process(null);

    return scheduler_handler(ctx, 0);
}

/**
 * Syscall handler for SYSCALL_YIELD. Sets ctx->x0 = 0 and delegates to
 * scheduler_handler to pick the next task.
 *
 * @param ctx: saved context of the yielding process
 *
 * @return saved context of the next task to run
 */
fn syscall_yield_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    ctx->x[0] = 0;

    dprintk("[scheduler] yield()\n");

    return scheduler_handler(ctx, 0);
}

/**
 * Syscall handler for SYSCALL_GETPID. Writes current->pid into ctx->x0.
 * Does not perform a context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx unchanged, with ctx->x0 set to the PID, or -1 if no process is
 *         currently scheduled
 */
fn syscall_getpid_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process for getpid!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    dprintk("[scheduler] getpid(%i)\n", proc->pid);

    ctx->x[0] = proc->pid as uint64;
    return ctx;
}

/**
 * Syscall handler for SYSCALL_WAITPID. Saves ctx into current->ctx, moves
 * current to the wait queue, and calls scheduler_handler to run the next task.
 * The caller is resumed by _notify_waiters when the target process exits, with
 * ctx->x0 set to the exit status.
 *
 * @param ctx: saved context of the blocking process (ctx->x1 holds the target PID)
 *
 * @return saved context of the next task to run
 */
@extern fn syscall_waitpid_handler(ctx: struct cpu_context*) -> struct cpu_context*;

/**
 * Syscall handler for SYSCALL_FORK. Saves ctx into current->ctx, duplicates
 * the current process (stack and context), and enqueues the child. The child
 * resumes from the same point with ctx->x0 = 0; the parent receives the
 * child's PID in ctx->x0.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx of the parent, with ctx->x0 set to the child's PID, or -1 on failure
 */
fn syscall_fork_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process to fork!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    proc->ctx = ctx;

    dprintk("[scheduler] fork(%i)\n", proc->pid);

    let child: struct process* = alloc<struct process>(1);
    duplicate_process(child, proc);
    scheduler_enqueue(child);

    child->ctx->x[0] = 0;
    ctx->x[0] = child->pid as uint64;

    return ctx;
}

/**
 * Syscall handler for SYSCALL_SLEEP. Sets current->sleep_for to
 * ctx->x1 * 1000 ms, moves current to the sleep queue, and calls
 * scheduler_handler to run the next task. The process is woken by
 * _notify_sleepers inside scheduler_handler once sleep_for reaches zero.
 *
 * @param ctx: saved context of the calling process; ctx->x1 holds the sleep
 *             duration in seconds
 *
 * @return ctx of the next task to run, or ctx unchanged if no process is
 *         currently scheduled
 */
@extern fn syscall_sleep_handler(ctx: struct cpu_context*) -> struct cpu_context*;

/**
 * Syscall handler for SYSCALL_MSLEEP. Sets current->sleep_for directly to
 * ctx->x1 ms, moves current to the sleep queue, and calls scheduler_handler
 * to run the next task. The process is woken by _notify_sleepers inside
 * scheduler_handler once sleep_for reaches zero.
 *
 * @param ctx: saved context of the calling process; ctx->x1 holds the sleep
 *             duration in milliseconds
 *
 * @return ctx of the next task to run, or ctx unchanged if no process is
 *         currently scheduled
 */
@extern fn syscall_msleep_handler(ctx: struct cpu_context*) -> struct cpu_context*;
