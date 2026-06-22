#pragma once

#include <sys/types.h>
#include <time.h>

#define NUM_SYSCALLS 256

#define SYSCALL_EXIT 0
#define SYSCALL_YIELD 1
#define SYSCALL_GETPID 2
#define SYSCALL_WAITPID 3
#define SYSCALL_FORK 4
#define SYSCALL_SLEEP 5
#define SYSCALL_MSLEEP 6
#define SYSCALL_TIME 7
#define SYSCALL_UPTIME 8

/**
 * Initializes the syscall subsystem.
 * Must be called before any syscall_register_handler or syscall_handler invocations. Initializes
 * the internal syscall dispatch table.
 */
void syscall_init();

/**
 * Voluntarily yields the CPU to the scheduler via SYSCALL_YIELD (svc #0).
 * Traps into EL1, where syscall_yield_handler sets the return value to 0 in ctx->x0 and calls
 * scheduler_handler to pick the next runnable task. Execution resumes in the calling task when it
 * is next scheduled.
 *
 * @return 0 on return from the scheduler.
 */
int64_t yield();

/**
 * Terminates the calling process via SYSCALL_EXIT (svc #0).
 * Traps into EL1, where syscall_exit_handler marks the process PROC_DEAD, destroys it, and calls
 * scheduler_handler to run the next task. Does not return.
 *
 * @param status: exit status passed in x1 (logged by syscall_exit_handler).
 */
void exit(int64_t status);

/**
 * Returns the PID of the calling process via SYSCALL_GETPID (svc #0).
 * Traps into EL1, where syscall_getpid_handler writes current->pid into ctx->x0.
 * Execution resumes in the calling task without a context switch.
 *
 * @return PID of the current process, or -1 if no process is currently scheduled.
 */
pid_t getpid();

/**
 * Blocks the calling process until the process with the given PID exits via SYSCALL_WAITPID (svc
 * #0). Traps into EL1, where syscall_waitpid_handler moves the caller to the wait queue and
 * performs a context switch. Execution resumes when the target process calls exit.
 *
 * @param pid: PID of the process to wait for
 *
 * @return exit status of the terminated process, or -1 if no process is
 *         currently scheduled.
 */
int64_t waitpid(pid_t pid);

/**
 * Forks the calling process via SYSCALL_FORK (svc #0). Traps into EL1, where syscall_fork_handler
 * duplicates the current process (stack and context) and enqueues the child. The child resumes from
 * the same point with a return value of 0; the parent receives the child's PID.
 *
 * @return child PID in the parent, 0 in the child, or -1 on failure.
 */
pid_t fork();

/**
 * Blocks the calling process for the given number of seconds via SYSCALL_SLEEP (svc #0). Traps into
 * EL1, where syscall_sleep_handler sets process->sleep_for and moves the caller to the sleep queue.
 * The timer tick decrements sleep_for on each tick; process is enqueued when it reaches zero.
 *
 * @param seconds: number of seconds to sleep
 *
 * @return 0 on wakeup, or -1 if no process is currently scheduled.
 */
int64_t sleep(time_t seconds);

/**
 * Blocks the calling process for the given number of milliseconds via SYSCALL_MSLEEP (svc #0).
 * Traps into EL1, where syscall_msleep_handler sets process->sleep_for directly to the given value
 * and moves the caller to the sleep queue. The timer tick decrements sleep_for on each tick;
 * process is enqueued when it reaches zero.
 *
 * @param ms: number of milliseconds to sleep
 *
 * @return 0 on wakeup, or -1 if no process is currently scheduled.
 */
int64_t msleep(mseconds_t ms);

/**
 * Returns the current Unix timestamp via SYSCALL_TIME (svc #0).
 * Traps into EL1, where syscall_time_handler reads the RTC and writes the value into ctx->x0.
 *
 * @return current Unix timestamp, or -1 on failure.
 */
time_t time();

/**
 * Returns the system uptime in milliseconds via SYSCALL_UPTIME (svc #0).
 * Traps into EL1, where syscall_uptime_handler reads cntpct_el0 and computes elapsed milliseconds
 * since timer_init.
 *
 * @return system uptime in milliseconds
 */
time_t uptime();
