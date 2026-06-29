import "debug";
import "cpu";
import "process";
import "queue";
import "set";
import "range";
import "mm";
import "filesystem/file";
import "interrupts/drivers/timer";

@static let idle_ctx: struct cpu_context* = null;
@static let idle_stack: byte[DEFAULT_STACK_SIZE];

@static let current: struct process* = null;

@static let ready_queue: struct queue<struct process*>;
@static let waitpid_queue: struct set<int64, struct process*>;
@static let sleep_queue: struct set<int64, struct process*>;

/**
 * Registers all scheduler syscall handlers. Must be called before irq_enable().
 *
 * Registered handlers:
 *   syscall::EXIT             → syscall_exit_handler
 *   syscall::YIELD            → syscall_yield_handler
 *   syscall::GETPID           → syscall_getpid_handler
 *   syscall::WAITPID          → syscall_waitpid_handler
 *   syscall::FORK             → syscall_fork_handler
 *   syscall::SLEEP            → syscall_sleep_handler
 *   syscall::MSLEEP           → syscall_msleep_handler
 *   syscall::OPEN             → syscall_open_handler
 *   syscall::CLOSE            → syscall_close_handler
 *   syscall::READ             → syscall_read_handler
 *   syscall::WRITE            → syscall_write_handler
 *   syscall::FSTAT            → syscall_fstat_handler
 *   syscall::STAT             → syscall_stat_handler
 *   syscall::STATAT           → syscall_statat_handler
 *   syscall::GETCWD           → syscall_getcwd_handler
 *   syscall::EXEC             → syscall_exec_handler
 *   syscall::EXECAT           → syscall_execat_handler
 */
fn scheduler_init() {
    idle_ctx = &idle_stack[DEFAULT_STACK_SIZE - 272] as struct cpu_context*;
    set_bytes(idle_ctx as byte*, 0, 272);

    queue_init(&ready_queue, 10);
    set_init(&waitpid_queue, 10);
    set_init(&sleep_queue, 10);

    syscall_register_handler(syscall::EXIT, syscall_exit_handler);
    syscall_register_handler(syscall::YIELD, syscall_yield_handler);
    syscall_register_handler(syscall::GETPID, syscall_getpid_handler);
    syscall_register_handler(syscall::WAITPID, syscall_waitpid_handler);
    syscall_register_handler(syscall::FORK, syscall_fork_handler);
    syscall_register_handler(syscall::SLEEP, syscall_sleep_handler);
    syscall_register_handler(syscall::MSLEEP, syscall_msleep_handler);
    syscall_register_handler(syscall::OPEN, syscall_open_handler);
    syscall_register_handler(syscall::OPENAT, syscall_openat_handler);
    syscall_register_handler(syscall::CLOSE, syscall_close_handler);
    syscall_register_handler(syscall::READ, syscall_read_handler);
    syscall_register_handler(syscall::WRITE, syscall_write_handler);
    syscall_register_handler(syscall::FSTAT, syscall_fstat_handler);
    syscall_register_handler(syscall::STAT, syscall_stat_handler);
    syscall_register_handler(syscall::STATAT, syscall_statat_handler);
    syscall_register_handler(syscall::GETCWD, syscall_getcwd_handler);
    syscall_register_handler(syscall::EXEC, syscall_exec_handler);
    syscall_register_handler(syscall::EXECAT, syscall_execat_handler);
}

/**
 * Sets proc->state to proc_state::READY and pushes it onto the tail of the ready queue.
 *
 * @param proc: process to enqueue; must be fully initialized (create_process
 *              and process_set_entry called)
 *
 * @return 0 on success
 */
fn scheduler_enqueue(proc: struct process*) -> int32 {
    proc->state = proc_state::READY;
    queue_push(&ready_queue, proc);

    let current_pid: int64 = 0;
    if (current != null)
        current_pid = current->pid;

    @if (DEBUG) {
        dprintk("[scheduler] enqueue(), q = { ", proc->pid);
        let r = struct range { end = ready_queue.length };
        for i in &r {
            let proc = queue_at(&ready_queue, i);
            dprintk("%lld ", proc->pid);
        }
        dprintk("}, current = %lld\n", current_pid);
    }
    return 0;
}

/**
 * Removes the head of the ready queue, marks the process proc_state::DEAD, and
 * returns it. Does not affect `current`.
 *
 * @return pointer to the dequeued process, or NULL if the queue is empty
 */
fn scheduler_dequeue() -> struct process* {
    if (ready_queue.length == 0)
        return null;

    let proc = queue_pop(&ready_queue);
    proc->state = proc_state::DEAD;

    dprintk("[scheduler] dequeue(%i)\n", proc->pid);

    return proc;
}

/**
 * Returns the process currently scheduled on this CPU, or null when none is
 * running (e.g. while the idle context executes).
 *
 * @return pointer to the running process, or null
 */
fn scheduler_get_current_process() -> struct process* {
    return current;
}

/**
 * Sets the process considered currently scheduled. Pass null to clear it (done
 * by the scheduler when a process blocks or exits).
 *
 * @param proc: process to mark as running, or null to clear
 */
fn scheduler_set_current_process(proc: struct process*) {
    current = proc;
}

/**
 * Allocates a process, configures it with the given entry point, and enqueues
 * it on the ready queue. Convenience wrapper around create_process,
 * process_set_entry, and scheduler_enqueue.
 *
 * @param entry: function to run as the process entry point
 *
 * @return PID of the newly spawned process
 */
fn scheduler_spawn(entry: fn ()) -> int64 {
    dprintk("[scheduler] spawn()\n");

    let proc: struct process* = knew<struct process>();

    dprintk("[scheduler] creating process\n");
    process_create(proc, DEFAULT_STACK_BLOCK_COUNT);

    dprintk("[scheduler] configuring process\n");
    process_set_entry(proc, entry);

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
fn scheduler_handler(ctx: struct cpu_context*, ms_elapsed: uint64) -> struct cpu_context* {
    if (ms_elapsed > 0 and sleep_queue.length > 0)
        notify_sleepers();

    let previous = scheduler_get_current_process();
    if (previous != null) {
        process_check_stack(previous);
        previous->ctx = ctx;
        queue_push(&ready_queue, previous);
    }

    if (ready_queue.length == 0) {
        dprintk("[scheduler] idle()\n");
        idle_ctx->elr = halt as uint64;
        idle_ctx->spsr = SPSR_EL1h;
        return idle_ctx;
    }

    let proc = queue_pop(&ready_queue);
    scheduler_set_current_process(proc);

    if (previous != null and previous->pid != proc->pid) {
        dprintk("[scheduler] context_switch(), q = { ");

        let procs = ready_queue.data as struct process**;
        
        let r = struct range { end = ready_queue.length };
        for i in &r {
            dprintk("%i ", procs[i]->pid);
        }

        dprintk("}, ");

        if (previous != null)
            dprintk("previous = %ld, ", previous->pid);
        else
            dprintk("previous = <null>, ");
        dprintk("current = %ld, ms_elapsed = %d ms\n", proc->pid, ms_elapsed);
    }

    return proc->ctx;
}

/**
 * Wakes every process in the sleep queue whose sleep_until has been reached,
 * clearing its sleep timer and re-enqueueing it on the ready queue. Called from
 * scheduler_handler on each timer tick while the sleep queue is non-empty.
 */
@private
fn notify_sleepers() {
    let uptime = timer_get_uptime();

    let proc = scheduler_get_current_process();
    let current_pid: int64 = 0;
    if (proc != null)
        current_pid = proc->pid;
    dprintk("[scheduler] __notify_sleepers(), current=%i\n", current_pid);

    for entry in &sleep_queue {
        let proc = entry.value;
        dprintk("[scheduler] pid=%i, sleep_until=%i, uptime=%i\n",
                proc->pid, proc->sleep_until, uptime);

        if (proc->sleep_until <= uptime) {
            dprintk("[scheduler] notifying pid %i\n", proc->pid);

            set_remove(&sleep_queue, proc->pid);

            proc->state = proc_state::READY;
            proc->sleep_until = 0;
            proc->ctx->x[0] = 0;

            queue_push(&ready_queue, proc);
        }
    }
}

/**
 * Wakes every process blocked in waitpid on wait_pid, delivering exit_status as
 * each waiter's return value and re-enqueueing it on the ready queue. Called by
 * syscall_exit_handler.
 *
 * @param wait_pid:    PID of the process that exited
 * @param exit_status: status to deliver to the waiters
 */
@private
fn notify_waiters(wait_pid: int64, exit_status: int64) {
    dprintk("[scheduler] notify_waiters(%ld), exit_status=%ld, waitpid_queue->length=%d\n",
            wait_pid, exit_status, waitpid_queue.length);

    for entry in &waitpid_queue {
        let proc = entry.value;
        dprintk("[scheduler] proc->pid=%i, proc->wait_pid=%i\n", proc->pid, proc->wait_pid);

        if (proc->wait_pid != wait_pid)
            continue;

        set_remove(&waitpid_queue, proc->pid);

        dprintk("[scheduler] _notify_waiter(%i), exit_status=%i\n", proc->pid, exit_status);

        proc->state = proc_state::READY;
        proc->wait_pid = 0;
        proc->ctx->x[0] = exit_status as uint64;

        queue_push(&ready_queue, proc);
    }
}

/**
 * Syscall handler for syscall::EXIT. Marks current proc_state::DEAD, notifies any
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

    dprintk("[scheduler] exit(%i), ctx->x0 = %llu, ctx->x1 = %llu\n", proc->pid, ctx->x[0], ctx->x[1]);

    proc->state = proc_state::DEAD;

    // notify waiters
    notify_waiters(proc->pid, ctx->x[1] as int64);

    // destroy process resources
    if (process_destroy(proc) < 0) {
        dprintk("[scheduler] failed to destroy process, addr = %p\n", proc);
    }

    // clear current entry
    scheduler_set_current_process(null);

    return scheduler_handler(ctx, 0);
}

/**
 * Syscall handler for syscall::YIELD. Sets ctx->x0 = 0 and delegates to
 * scheduler_handler to pick the next task.
 *
 * @param ctx: saved context of the yielding process
 *
 * @return saved context of the next task to run
 */
fn syscall_yield_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    ctx->x[0] = 0;

    dprintk("[scheduler] yield(), ctx->x0 = %llu\n", ctx->x[0]);

    return scheduler_handler(ctx, 0);
}

/**
 * Syscall handler for syscall::GETPID. Writes current->pid into ctx->x0.
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

    dprintk("[scheduler] getpid(%i), ctx->x0 = %llu, ctx->x1 = %llu\n", proc->pid, ctx->x[0], ctx->x[1]);

    ctx->x[0] = proc->pid as uint64;
    return ctx;
}

/**
 * Syscall handler for syscall::WAITPID. Saves ctx into current->ctx, moves
 * current to the wait queue, and calls scheduler_handler to run the next task.
 * The caller is resumed by _notify_waiters when the target process exits, with
 * ctx->x0 set to the exit status.
 *
 * @param ctx: saved context of the blocking process (ctx->x1 holds the target PID)
 *
 * @return saved context of the next task to run
 */
fn syscall_waitpid_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let pid = ctx->x[1] as int64;

    dprintk("[scheduler] waitpid(), ctx->x0 = %llu, ctx->x1 = %llu\n", ctx->x[0], ctx->x[1]);

    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process for waitpid!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    proc->state = proc_state::BLOCKED;
    proc->wait_pid = pid;
    proc->ctx = ctx;

    set_set(&waitpid_queue, proc->pid, proc);
    scheduler_set_current_process(null);

    return scheduler_handler(ctx, 0);
}

/**
 * Syscall handler for syscall::FORK. Saves ctx into current->ctx, duplicates
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

    dprintk("[scheduler] fork(%i), ctx->x0 = %llu\n", proc->pid, ctx->x[0]);

    let child: struct process* = knew<struct process>();
    if (process_duplicate(child, proc) < 0) {
        kdealloc(child);
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    scheduler_enqueue(child);

    child->ctx->x[0] = 0;
    ctx->x[0] = child->pid as uint64;

    return ctx;
}

/**
 * Syscall handler for syscall::SLEEP. Sets current->sleep_for to
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
fn syscall_sleep_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let seconds = ctx->x[1];

    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process for sleep!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    dprintk("[scheduler] sleep(), ctx->x0 = %llu, ctx->x1 = %llu\n", ctx->x[0], ctx->x[1]);

    let sleep_duration = seconds * 1000;
    proc->state = proc_state::BLOCKED;
    proc->sleep_until = timer_get_uptime() + sleep_duration;
    proc->ctx = ctx;

    set_set(&sleep_queue, proc->pid, proc);
    scheduler_set_current_process(null);

    return scheduler_handler(ctx, 0);
}

/**
 * Syscall handler for syscall::MSLEEP. Sets current->sleep_for directly to
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
fn syscall_msleep_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let mseconds = ctx->x[1];

    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process for msleep!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    dprintk("[scheduler] msleep(), ctx->x0 = %llu, ctx->x1 = %llu\n", ctx->x[0], ctx->x[1]);

    proc->state = proc_state::BLOCKED;
    proc->sleep_until = timer_get_uptime() + mseconds;
    proc->ctx = ctx;

    set_set(&sleep_queue, proc->pid, proc);
    scheduler_set_current_process(null);

    return scheduler_handler(ctx, 0);
}

/**
 * Handles syscall::OPEN. Opens the path in x1 (with access mode in x2) in the
 * current process's fd table via process_open_file and returns the descriptor in
 * x0. No context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx with x0 set to the new fd, or -1 on failure
 */
fn syscall_open_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let path = ctx->x[1] as char*;
    let attrs = ctx->x[2] as uint32;

    dprintk("[scheduler] open(), ctx->x0 = %llu, ctx->x1 = 0x%p, ctx->x2 = %llu\n",
           ctx->x[0], ctx->x[1], ctx->x[2]);

    ctx->x[0] = process_open_file(proc, path, attrs) as uint64;

    return ctx;
}

fn syscall_openat_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let dirfd = ctx->x[1] as int64;
    let path = ctx->x[2] as char*;
    let attrs = ctx->x[3] as uint32;

    dprintk("[scheduler] openat(), ctx->x0 = %llu, ctx->x1 = %llu, ctx->x2 = 0x%p, ctx->x3 = %llu\n",
           ctx->x[0], ctx->x[1], ctx->x[2], ctx->x[3]);

    ctx->x[0] = process_open_file_at(proc, dirfd, path, attrs) as uint64;

    return ctx;
}

/**
 * Handles syscall::CLOSE. Closes the descriptor in x1 in the current process's fd
 * table via process_close_file and returns the result in x0. No context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx with x0 set to 0 on success, or -1 on failure
 */
fn syscall_close_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let fd = ctx->x[1] as int64;

    dprintk("[scheduler] close(), ctx->x0 = %llu, ctx->x1 = %llu\n",
            ctx->x[0], ctx->x[1]);

    ctx->x[0] = process_close_file(proc, fd) as uint64;

    return ctx;
}

/**
 * Handles syscall::READ. Reads count (x3) bytes from descriptor fd (x1) into the
 * buffer at x2 via process_read_file and returns the byte count in x0. No context
 * switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx with x0 set to the number of bytes read, or a negative error
 */
fn syscall_read_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    dprintk("[scheduler] read(), ctx->x0 = %llu, ctx->x1 = %llu, ctx->x2 = %llu, ctx->x3 = %llu\n",
            ctx->x[0], ctx->x[1], ctx->x[2], ctx->x[3]);

    let fd = ctx->x[1] as int64;
    let buffer = ctx->x[2] as byte*;
    let count = ctx->x[3] as uint64;

    ctx->x[0] = process_read_file(proc, fd, buffer, count) as uint64;

    return ctx;
}

/**
 * Handles syscall::WRITE. Writes count (x3) bytes from the buffer at x2 to
 * descriptor fd (x1) via process_write_file and returns the byte count in x0. No
 * context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx with x0 set to the number of bytes written, or a negative error
 */
fn syscall_write_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    dprintk("[scheduler] write(), ctx->x0 = %llu, ctx->x1 = %llu, ctx->x2 = %llu, ctx->x3 = %llu\n",
            ctx->x[0], ctx->x[1], ctx->x[2], ctx->x[3]);

    let fd = ctx->x[1] as int64;
    let buffer = ctx->x[2] as byte*;
    let count = ctx->x[3] as uint64;

    ctx->x[0] = process_write_file(proc, fd, buffer, count) as uint64;

    return ctx;
}

/**
 * Handles syscall::FSTAT. Fills the file_stat at x2 with metadata for descriptor
 * fd (x1) via process_file_stat and returns the result in x0. No context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx with x0 set to 0 on success, or a negative error
 */
fn syscall_fstat_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let fd = ctx->x[1] as int64;
    let st = ctx->x[2] as struct file_stat*;

    dprintk("[scheduler] fstat(), ctx->x0 = %llu, ctx->x1 = %llu ctx->x2 = %llu\n",
            ctx->x[0], ctx->x[1], ctx->x[2]);
    
    ctx->x[0] = process_file_stat(proc, fd, st) as uint64;
    
    return ctx;
}

/**
 * Handles syscall::STAT. Fills the file_stat at x2 with metadata for the file at
 * the path in x1 (resolved relative to the process cwd) via process_stat and
 * returns the result in x0. Unlike fstat, no open descriptor is needed. No
 * context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx with x0 set to 0 on success, or a negative error
 */
fn syscall_stat_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let path = ctx->x[1] as char*;
    let st = ctx->x[2] as struct file_stat*;

    dprintk("[scheduler] stat(), ctx->x0 = %llu, ctx->x1 = %p, ctx->x2 = %llu\n",
            ctx->x[0], ctx->x[1], ctx->x[2]);

    ctx->x[0] = process_stat(proc, path, st) as uint64;

    return ctx;
}

/**
 * Handles syscall::STATAT. Like syscall_stat_handler, but resolves the path in x2
 * relative to the open directory descriptor dirfd (x1) via process_stat_at and
 * fills the file_stat at x3. Returns the result in x0. No context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx with x0 set to 0 on success, or a negative error
 */
fn syscall_statat_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let dirfd = ctx->x[1] as int64;
    let path = ctx->x[2] as char*;
    let st = ctx->x[3] as struct file_stat*;

    dprintk("[scheduler] statat(), ctx->x0 = %llu, ctx->x1 = %llu, ctx->x2 = %p, ctx->x3 = %lld\n",
            ctx->x[0], ctx->x[1], ctx->x[2], ctx->x[3]);

    ctx->x[0] = process_stat_at(proc, dirfd, path, st) as uint64;

    return ctx;
}

/**
 * Handles syscall::GETCWD. Writes the absolute path of the process's current
 * working directory into the buffer at x1 (capacity x2) via process_get_cwd and
 * returns the result in x0. No context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx with x0 set to the path length, or -1 on failure
 */
fn syscall_getcwd_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let buf = ctx->x[1] as byte*;
    let size = ctx->x[2] as uint64;

    dprintk("[scheduler] getcwd(), ctx->x0 = %llu, ctx->x1 = %llu, ctx->x2 = %llu\n",
            ctx->x[0], ctx->x[1], ctx->x[2]);

    ctx->x[0] = process_get_cwd(proc, buf, size) as uint64;

    return ctx;
}

/**
 * Handles syscall::EXEC. Replaces the calling process's image with the program at
 * path (x1), passing argc (x2) and argv (x3) via process_exec, then resumes the
 * process on its (possibly rebuilt) context. On success the old image is gone, so
 * x0 carries the negative error code only on failure.
 *
 * @param ctx: saved context of the calling process
 *
 * @return the process's context to resume (proc->ctx), with x0 = a negative error
 *         on failure
 */
fn syscall_exec_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let path = ctx->x[1] as char*;
    let argc = ctx->x[2] as int64;
    let argv = ctx->x[3] as char**;
    
    dprintk("[scheduler] exec(), ctx->x0 = %lld, ctx->x1 = %p, ctx->x2 = %lld, ctx->x3 = %p\n",
            ctx->x[0], ctx->x[1], ctx->x[2], ctx->x[3]);

    let status = process_exec(proc, path, argc, argv);
    if (status < 0) {
        ctx->x[0] = status as uint64;
        return ctx;
    }
    
    return proc->ctx;
}

/**
 * Handles syscall::EXECAT. Like syscall_exec_handler, but resolves the program
 * path (x2) relative to the open directory descriptor dirfd (x1), passing argc
 * (x3) and argv (x4) via process_exec_at, then resumes the process on its rebuilt
 * context. On success the old image is gone, so x0 carries the negative error
 * code only on failure.
 *
 * @param ctx: saved context of the calling process
 *
 * @return the process's context to resume (proc->ctx), with x0 = a negative error
 *         on failure
 */
fn syscall_execat_handler(ctx: struct cpu_context*) -> struct cpu_context* {
    let proc = scheduler_get_current_process();
    if (proc == null) {
        dprintk("[scheduler] no current process!\n");
        ctx->x[0] = -1 as uint64;
        return ctx;
    }

    let dirfd = ctx->x[1] as int64;
    let path = ctx->x[2] as char*;
    let argc = ctx->x[3] as int64;
    let argv = ctx->x[4] as char**;

    dprintk("[scheduler] execat(), dirfd = %lld, path = %p, argc = %lld, argv = %p\n",
            ctx->x[1], ctx->x[2], ctx->x[3], ctx->x[4]);

    let status = process_exec_at(proc, dirfd, path, argc, argv);
    if (status < 0) {
        ctx->x[0] = status as uint64;
        return ctx;
    }

    return proc->ctx;
}
