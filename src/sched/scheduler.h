#pragma once

#include "process.h"

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
void scheduler_init();

/**
 * Allocates a process, configures it with the given entry point, and enqueues
 * it on the ready queue. Convenience wrapper around create_process,
 * process_set_entry, and scheduler_enqueue.
 *
 * @param entry: function to run as the process entry point
 *
 * @return PID of the newly spawned process
 */
pid_t scheduler_spawn(proc_entry entry);