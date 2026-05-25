#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <arch/cpu.h>
#include <arch/irq.h>
#include "process.h"

struct scheduler_entry
{
    struct process *proc;
    struct scheduler_entry *prev;
    struct scheduler_entry *next;
};

/**
 * Registers the SGI yield handler with the GIC.
 * Must be called after gic_init() and before irq_enable().
 */
void scheduler_init();

/**
 * Adds a process to the tail of the run queue.
 *
 * @param proc: process to enqueue; must be fully initialized (ctx, entry set)
 */
void scheduler_enqueue(struct process *proc);

/**
 * Round-robin scheduler. Advances to the next PROC_READY process in the run
 * queue and returns its saved context pointer.
 * Registered as the timer IRQ callback to preempt the current task each tick.
 *
 * @param ctx: saved context of the currently running process
 *
 * @return saved context of the next process to run; equals ctx if no other
 *         PROC_READY process exists in the queue.
 */
struct cpu_context *scheduler_handler(struct cpu_context *ctx);

/**
 * Performs an immediate context switch by delegating to scheduler_handler.
 * Registered as the handler for both SGI 0 (gic_yield) and the yield syscall
 * (syscall_register_handler at scheduler_init time).
 *
 * @param ctx: saved context of the currently running process
 *
 * @return saved context of the next process to run
 */
struct cpu_context *yield_handler(struct cpu_context *ctx);

#endif // SCHEDULER_H