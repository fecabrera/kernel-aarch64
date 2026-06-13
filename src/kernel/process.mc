import "cpu";
import "memory";

const DEFAULT_STACK_SIZE = (1 << 14); // 16 KiB
const PROC_DEAD = -1;
const PROC_CREATED = 0;
const PROC_READY = 1;
const PROC_BLOCKED = 2;

@static let next_pid: int64 = 1;

/**
 * Represents a schedulable process. Created by create_process, configured by
 * process_config, and destroyed by destroy_process.
 *
 * @field pid:         (pid_t) unique process identifier, assigned at creation
 * @field state:       current lifecycle state (PROC_CREATED, PROC_READY, PROC_BLOCKED, or
 *                     PROC_DEAD)
 * @field stack:       base address of the heap-allocated task stack
 * @field ctx:         saved register frame (cpu_context), embedded near the top of stack and
 *                     updated on every context switch
 * @field stack_size:  size of the stack allocation in bytes
 * @field wait_pid:    (pid_t) PID this process is blocked waiting for via waitpid; 0 when not
 *                     waiting
 * @field sleep_until: (time_t) absolute uptime in milliseconds at which the process should wake; 0
 *                     when not sleeping
 */
struct process {
    pid: int64;
    state: int64;
    stack: uint8*;
    ctx: struct cpu_context*;
    stack_size: uint64;
    wait_pid: int64;
    sleep_until: uint64;
}

/**
 * Allocates a stack and initializes the process struct with a zeroed context
 * frame. The process is left in PROC_CREATED state; call process_config
 * before enqueueing.
 *
 * @param proc: caller-allocated process struct to initialize
 * @param stack_size: size in bytes of the task stack to allocate
 *
 * @return 0 on success, -1 if stack allocation fails
 */
fn create_process(proc: struct process*, stack_size: uint64) -> int32 {
    let stack: uint8* = alloc_aligned<uint8>(stack_size, 16);

    if (stack == null) {
        return -1;
    }

    // vectors.S save_context uses a 272-byte frame (sizeof(cpu_context)=264 + 8
    // bytes padding for 16-byte SP alignment). Place ctx 272 bytes from the top
    // so that after restore_context's "add sp, sp, #272", sp == stack + stack_size
    // (16-byte aligned).
    let ctx = (stack as uint64 + stack_size - 272) as struct cpu_context*;
    set_bytes(ctx, 0, 1);

    proc->pid = next_pid;
    proc->state = PROC_CREATED;
    proc->ctx = ctx;
    proc->stack = stack;
    proc->stack_size = stack_size;

    next_pid = next_pid + 1;

    return 0;
}

/**
 * Allocates a new stack for dest and copies src's stack contents and context
 * frame into it, preserving the ctx offset within the stack. Assigns dest a
 * new PID and sets its state to PROC_CREATED. Call process_config or adjust
 * dest->ctx->x0 before enqueueing.
 *
 * @param dest: caller-allocated process struct to initialize
 * @param src:  process to copy from
 *
 * @return 0 on success, -1 if stack allocation fails
 */
fn duplicate_process(dest: struct process*, src: struct process*) -> int32 {
    let stack_size = src->stack_size;
    let stack: uint8* = alloc_aligned<uint8>(stack_size, 16);

    if (stack == null)
        return -1;

    let ctx_offset: uint64 = (src->ctx as uint64) - (src->stack as uint64);
    let ctx = (stack as uint64 + ctx_offset) as struct cpu_context*;
    copy_bytes(ctx, src->ctx, 1);

    dest->pid = next_pid;
    dest->state = PROC_CREATED;
    dest->ctx = ctx;
    dest->stack = stack;
    dest->stack_size = stack_size;

    next_pid = next_pid + 1;

    return 0;
}

/**
 * Configures the process entry point and initial SPSR.
 * Must be called before the process is enqueued in the scheduler.
 *
 * @param proc:  process to configure
 * @param entry: function the process will execute after its first eret
 */
fn process_config(proc: struct process*, entry: fn ()) {
    let ctx: struct cpu_context* = proc->ctx;
    ctx->elr = entry as uint64;
    ctx->spsr = SPSR_EL1h;
}

/**
 * Frees the task stack and nulls out stack and ctx pointers.
 * Requires proc->state == PROC_DEAD.
 *
 * @param proc: process to destroy; must be in PROC_DEAD state
 *
 * @return 0 on success, -1 if proc->state != PROC_DEAD
 */
fn destroy_process(proc: struct process*) -> int32 {
    if (proc->state != PROC_DEAD)
        return -1;

    dealloc(proc->stack);

    proc->stack = null;
    proc->ctx = null;

    return 0;
}