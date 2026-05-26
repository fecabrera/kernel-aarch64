#ifndef SCHEDULER_H
#define SCHEDULER_H

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
 */
void scheduler_init();

/**
 * Allocates a queue entry for proc, sets its state to PROC_READY, and adds
 * it to the tail of the ready queue.
 *
 * @param proc: process to enqueue; must be fully initialized (create_process
 *              and process_config called)
 *
 * @return 0 on success, -1 on allocation failure
 */
int scheduler_enqueue(struct process *proc);

/**
 * Removes the head of the run queue, marks the process PROC_DEAD, frees the
 * scheduler entry, and returns the process pointer. Does not affect `current`.
 *
 * @return pointer to the dequeued process, or NULL if the queue is empty
 */
struct process *scheduler_dequeue();

/**
 * FIFO scheduler. Re-enqueues `current` (if non-NULL) at the tail, then
 * dequeues the head and sets it as the new `current`.
 * Called by timer_irq_handler on each tick and by yield/exit handlers.
 *
 * @param ctx: saved context of the calling task (used as fallback if queue is empty)
 *
 * @return saved context of the next task to run; equals ctx if the queue is empty.
 */
struct cpu_context *scheduler_handler(struct cpu_context *ctx);

/**
 * Syscall handler for SYSCALL_EXIT. Terminates `current`: marks it PROC_DEAD,
 * destroys its stack, nulls `current`, then calls scheduler_handler.
 * Returns ctx unchanged if no process is currently running.
 *
 * @param ctx: saved context of the exiting process (ctx->x1 holds the status)
 *
 * @return saved context of the next task to run.
 */
struct cpu_context *exit_handler(struct cpu_context *ctx);

/**
 * Syscall handler for SYSCALL_YIELD. Sets ctx->x0 = 0 (return value seen by
 * the caller) and delegates to scheduler_handler to pick the next task.
 *
 * @param ctx: saved context of the yielding process
 *
 * @return saved context of the next task to run.
 */
struct cpu_context *yield_handler(struct cpu_context *ctx);

/**
 * Syscall handler for SYSCALL_GETPID. Writes current->proc->pid into ctx->x0.
 * Returns -1 in ctx->x0 if no process is currently scheduled.
 * Does not perform a context switch.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx unchanged.
 */
struct cpu_context *getpid_handler(struct cpu_context *ctx);

#endif // SCHEDULER_H