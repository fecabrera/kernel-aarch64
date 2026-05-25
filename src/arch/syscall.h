#ifndef SYSCALL_H
#define SYSCALL_H

#include "irq.h"

#define NUM_SYSCALLS 256

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
 * Issues a syscall by loading syscall_id into x0 and executing svc #0.
 * Traps into EL1 via the sync exception vector and dispatches through
 * syscall_handler.
 *
 * @param syscall_id: syscall number to invoke (0–NUM_SYSCALLS-1)
 */
void syscall(uint64_t syscall_id);

#endif // SYSCALL_H