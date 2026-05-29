#pragma once

#include <arch/cpu.h>
#include <arch/irq.h>
#include "process.h"

/**
 * Registers all scheduler syscall handlers. Must be called before irq_enable().
 *
 * Registered handlers:
 *   SYSCALL_EXIT             → exit_handler
 *   SYSCALL_YIELD            → yield_handler
 *   SYSCALL_GETPID           → getpid_handler
 *   SYSCALL_WAITPID          → waitpid_handler
 *   SYSCALL_FORK             → fork_handler
 *   SYSCALL_SLEEP            → sleep_handler
 */
void scheduler_init();

/**
 * Sets proc->state to PROC_READY and pushes it onto the tail of the ready queue.
 *
 * @param proc: process to enqueue; must be fully initialized (create_process
 *              and process_config called)
 *
 * @return 0 on success
 */
int scheduler_enqueue(struct process *proc);

/**
 * Removes the head of the ready queue, marks the process PROC_DEAD, and
 * returns it. Does not affect `current`.
 *
 * @return pointer to the dequeued process, or NULL if the queue is empty
 */
struct process *scheduler_dequeue();

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
struct cpu_context *scheduler_handler(struct cpu_context *ctx, time_t ms_elapsed);

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
struct cpu_context *exit_handler(struct cpu_context *ctx);

/**
 * Syscall handler for SYSCALL_YIELD. Sets ctx->x0 = 0 and delegates to
 * scheduler_handler to pick the next task.
 *
 * @param ctx: saved context of the yielding process
 *
 * @return saved context of the next task to run
 */
struct cpu_context *yield_handler(struct cpu_context *ctx);

/**
 * Syscall handler for SYSCALL_GETPID. Writes current->pid into ctx->x0.
 * Does not perform a context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx unchanged, with ctx->x0 set to the PID, or -1 if no process is
 *         currently scheduled
 */
struct cpu_context *getpid_handler(struct cpu_context *ctx);

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
struct cpu_context *waitpid_handler(struct cpu_context *ctx);

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
struct cpu_context *fork_handler(struct cpu_context *ctx);

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
struct cpu_context *sleep_handler(struct cpu_context *ctx);
