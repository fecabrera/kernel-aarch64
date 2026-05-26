#ifndef SYSCALL_H
#define SYSCALL_H

#include "irq.h"

#define NUM_SYSCALLS 256

#define SYSCALL_EXIT 0
#define SYSCALL_YIELD 1
#define SYSCALL_GETPID 2

/**
 * Dispatches a syscall based on the number in ctx->x0.
 * Called from sync_handler when ESR_EC_SVC64 is detected.
 *
 * @param ctx: saved context of the calling process
 *
 * @return pointer to the saved cpu_context of the next task to run;
 *         may equal ctx if no context switch is needed.
 */
struct cpu_context *syscall_handler(struct cpu_context *ctx);

/**
 * Registers a handler for the given syscall ID.
 *
 * @param syscall_id: syscall number to handle (0–NUM_SYSCALLS-1)
 * @param fnc: function to call when the syscall is invoked
 */
void syscall_register_handler(uint64_t syscall_id, interrupt_handler fnc);

/**
 * Removes the handler for the given syscall ID.
 * After this call, invoking the syscall will return ctx unchanged.
 *
 * @param syscall_id: syscall number to unregister (0–NUM_SYSCALLS-1)
 */
void syscall_unregister_handler(uint64_t syscall_id);

/**
 * Syscall handler for SYSCALL_GETPID. Returns the PID of the currently
 * running process by writing current->proc->pid into ctx->x0.
 * Returns -1 in ctx->x0 if no process is currently scheduled.
 *
 * @param ctx: saved context of the calling process
 *
 * @return ctx unchanged (no context switch).
 */
struct cpu_context *getpid_handler(struct cpu_context *ctx);

/**
 * Voluntarily yields the CPU to the scheduler via SYSCALL_YIELD (svc #0).
 * Traps into EL1, where yield_handler sets the return value to 0 in ctx->x0
 * and calls scheduler_handler to pick the next runnable task. Execution
 * resumes in the calling task when it is next scheduled.
 *
 * @return 0 on return from the scheduler.
 */
int64_t syscall_yield();

/**
 * Terminates the calling process via SYSCALL_EXIT (svc #0).
 * Traps into EL1, where exit_handler marks the process PROC_DEAD, destroys
 * it, and calls scheduler_handler to run the next task. Does not return.
 *
 * @param status: exit status passed in x1 (logged by exit_handler).
 */
void syscall_exit(uint64_t status);

/**
 * Returns the PID of the calling process via SYSCALL_GETPID (svc #0).
 * Traps into EL1, where getpid_handler writes current->proc->pid into
 * ctx->x0. Execution resumes in the calling task without a context switch.
 *
 * @return PID of the current process, or (uint64_t)-1 if no process is
 *         currently scheduled.
 */
uint64_t syscall_getpid();

#endif // SYSCALL_H