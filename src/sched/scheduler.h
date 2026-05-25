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
 * Adds a process to the tail of the run queue.
 *
 * @param proc: process to enqueue; must be fully initialized (ctx, entry set)
 */
void scheduler_enqueue(struct process *proc);

/**
 * Round-robin scheduler. Advances to the next ready process in the run queue
 * and returns its saved context pointer.
 * Registered as the timer IRQ callback to preempt the current task each tick.
 *
 * @param ctx: saved context of the currently running process
 *
 * @return saved context of the next process to run; may equal ctx if only one
 *         process is in the queue.
 */
struct cpu_context *scheduler_handler(struct cpu_context *ctx);

#endif // SCHEDULER_H