const NUM_SYSCALLS = 256;

const SYSCALL_EXIT = 0;
const SYSCALL_YIELD = 1;
const SYSCALL_GETPID = 2;
const SYSCALL_WAITPID = 3;
const SYSCALL_FORK = 4;
const SYSCALL_SLEEP = 5;
const SYSCALL_MSLEEP = 6;
const SYSCALL_TIME = 7;
const SYSCALL_UPTIME = 8;

/**
 * Voluntarily yields the CPU to the scheduler via SYSCALL_YIELD (svc #0).
 * Traps into EL1, where yield_handler sets the return value to 0 in ctx->x0 and calls
 * scheduler_handler to pick the next runnable task. Execution resumes in the calling task when it
 * is next scheduled.
 *
 * @return 0 on return from the scheduler.
 */
@extern fn syscall_yield() -> int64;

/**
 * Terminates the calling process via SYSCALL_EXIT (svc #0).
 * Traps into EL1, where exit_handler marks the process PROC_DEAD, destroys it, and calls
 * scheduler_handler to run the next task. Does not return.
 *
 * @param status: exit status passed in x1 (logged by exit_handler).
 */
@extern fn syscall_exit(status: int64);

/**
 * Returns the PID of the calling process via SYSCALL_GETPID (svc #0).
 * Traps into EL1, where getpid_handler writes current->pid into ctx->x0.
 * Execution resumes in the calling task without a context switch.
 *
 * @return PID of the current process, or -1 if no process is currently scheduled.
 */
@extern fn syscall_getpid() -> int64;

/**
 * Blocks the calling process until the process with the given PID exits via SYSCALL_WAITPID (svc
 * #0). Traps into EL1, where waitpid_handler moves the caller to the wait queue and performs a
 * context switch. Execution resumes when the target process calls syscall_exit.
 *
 * @param pid: PID of the process to wait for
 *
 * @return exit status of the terminated process, or -1 if no process is
 *         currently scheduled.
 */
@extern fn syscall_waitpid(pid: int64) -> int64;

/**
 * Forks the calling process via SYSCALL_FORK (svc #0). Traps into EL1, where fork_handler
 * duplicates the current process (stack and context) and enqueues the child. The child resumes from
 * the same point with a return value of 0; the parent receives the child's PID.
 *
 * @return child PID in the parent, 0 in the child, or -1 on failure.
 */
@extern fn syscall_fork() -> int64;

/**
 * Blocks the calling process for the given number of seconds via SYSCALL_SLEEP (svc #0). Traps into
 * EL1, where sleep_handler sets process->sleep_for and moves the caller to the sleep queue. The
 * timer tick decrements sleep_for on each tick; process is enqueued when it reaches zero.
 *
 * @param s: number of seconds to sleep
 *
 * @return 0 on wakeup, or -1 if no process is currently scheduled.
 */
@extern fn syscall_sleep(s: uint64) -> int64;

/**
 * Blocks the calling process for the given number of milliseconds via SYSCALL_MSLEEP (svc #0).
 * Traps into EL1, where msleep_handler sets process->sleep_for directly to the given value and
 * moves the caller to the sleep queue. The timer tick decrements sleep_for on each tick; process is
 * enqueued when it reaches zero.
 *
 * @param ms: number of milliseconds to sleep
 *
 * @return 0 on wakeup, or -1 if no process is currently scheduled.
 */
@extern fn syscall_msleep(ms: uint64) -> int64;

/**
 * Returns the current Unix timestamp via SYSCALL_TIME (svc #0).
 * Traps into EL1, where time_handler reads the RTC and writes the value into ctx->x0.
 *
 * @return current Unix timestamp, or -1 on failure.
 */
@extern fn syscall_time() -> uint64;

/**
 * Returns the system uptime in milliseconds via SYSCALL_UPTIME (svc #0).
 * Traps into EL1, where syscall_uptime_handler reads cntpct_el0 and computes elapsed milliseconds
 * since timer_init.
 *
 * @return system uptime in milliseconds
 */
@extern fn syscall_uptime() -> uint64;
