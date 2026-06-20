import "debug";
import "filesystem/file";
import "../syscall";

/**
 * Voluntarily yields the CPU to the scheduler via SYSCALL_YIELD (svc #0).
 * Traps into EL1, where syscall_yield_handler sets the return value to 0 in ctx->x0 and calls
 * scheduler_handler to pick the next runnable task. Execution resumes in the calling task when it
 * is next scheduled.
 *
 * @return 0 on return from the scheduler.
 */
@inline fn yield() -> int64 {
    dprintk("[syscall] yield()\n");
    return @asm @clobbers("x0", "memory") (SYSCALL_YIELD) -> int64 {
        "mov x0, $0"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Terminates the calling process via SYSCALL_EXIT (svc #0).
 * Traps into EL1, where syscall_exit_handler marks the process PROC_DEAD, destroys it, and calls
 * scheduler_handler to run the next task. Does not return.
 *
 * @param status: exit status passed in x1 (logged by syscall_exit_handler).
 */
@inline fn exit(status: int64) {
    dprintk("[syscall] exit(%lld)\n", status);
    @asm @clobbers("x0", "x1", "memory") (SYSCALL_EXIT, status) {
        "mov x0, $0"
        "mov x1, $1"
        "svc #0"
    };
}

/**
 * Returns the PID of the calling process via SYSCALL_GETPID (svc #0).
 * Traps into EL1, where syscall_getpid_handler writes current->pid into ctx->x0.
 * Execution resumes in the calling task without a context switch.
 *
 * @return PID of the current process, or -1 if no process is currently scheduled.
 */
@inline fn getpid() -> int64 {
    dprintk("[syscall] getpid()\n");
    return @asm @clobbers("x0") (SYSCALL_GETPID) -> int64 {
        "mov x0, $0"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Blocks the calling process until the process with the given PID exits via SYSCALL_WAITPID (svc
 * #0). Traps into EL1, where syscall_waitpid_handler moves the caller to the wait queue and performs a
 * context switch. Execution resumes when the target process calls exit.
 *
 * @param pid: PID of the process to wait for
 *
 * @return exit status of the terminated process, or -1 if no process is
 *         currently scheduled.
 */
@inline fn waitpid(pid: int64) -> int64 {
    dprintk("[syscall] waitpid(%lld)\n", pid);
    return @asm @clobbers("x0", "x1", "memory") (SYSCALL_WAITPID, pid) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Forks the calling process via SYSCALL_FORK (svc #0). Traps into EL1, where syscall_fork_handler
 * duplicates the current process (stack and context) and enqueues the child. The child resumes from
 * the same point with a return value of 0; the parent receives the child's PID.
 *
 * @return child PID in the parent, 0 in the child, or -1 on failure.
 */
@inline fn fork() -> int64 {
    dprintk("[syscall] fork()\n");
    return @asm @clobbers("x0", "memory") (SYSCALL_FORK) -> int64 {
        "mov x0, $0"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Blocks the calling process for the given number of seconds via SYSCALL_SLEEP (svc #0). Traps into
 * EL1, where syscall_sleep_handler sets process->sleep_for and moves the caller to the sleep queue. The
 * timer tick decrements sleep_for on each tick; process is enqueued when it reaches zero.
 *
 * @param s: number of seconds to sleep
 *
 * @return 0 on wakeup, or -1 if no process is currently scheduled.
 */
@inline fn sleep(seconds: uint64) -> int64 {
    dprintk("[syscall] sleep(%llu)\n", seconds);
    return @asm @clobbers("x0", "x1", "memory") (SYSCALL_SLEEP, seconds) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Blocks the calling process for the given number of milliseconds via SYSCALL_MSLEEP (svc #0).
 * Traps into EL1, where syscall_msleep_handler sets process->sleep_for directly to the given value and
 * moves the caller to the sleep queue. The timer tick decrements sleep_for on each tick; process is
 * enqueued when it reaches zero.
 *
 * @param ms: number of milliseconds to sleep
 *
 * @return 0 on wakeup, or -1 if no process is currently scheduled.
 */
@inline fn msleep(mseconds: uint64) -> int64 {
    dprintk("[syscall] msleep(%llu)\n", mseconds);
    return @asm @clobbers("x0", "x1", "memory") (SYSCALL_MSLEEP, mseconds) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Returns the current Unix timestamp via SYSCALL_TIME (svc #0).
 * Traps into EL1, where syscall_time_handler reads the RTC and writes the value into ctx->x0.
 *
 * @return current Unix timestamp, or -1 on failure.
 */
@inline fn time() -> uint64 {
    return @asm @clobbers("x0", "memory") (SYSCALL_TIME) -> uint64 {
        "mov x0, $0"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Returns the system uptime in milliseconds via SYSCALL_UPTIME (svc #0).
 * Traps into EL1, where syscall_uptime_handler reads cntpct_el0 and computes elapsed milliseconds
 * since timer_init.
 *
 * @return system uptime in milliseconds
 */
@inline fn uptime() -> uint64 {
    return @asm @clobbers("x0", "memory") (SYSCALL_UPTIME) -> uint64 {
        "mov x0, $0"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Opens path via SYSCALL_OPEN (svc #0). Traps into EL1, where syscall_open_handler
 * resolves the path relative to the process cwd and adds an entry to its fd table.
 *
 * @param path:  null-terminated path to open
 * @param attrs: access mode (FS_FILE_ATTRS_READ/WRITE/EXEC bits)
 *
 * @return the new file descriptor, or -1 on failure.
 */
@inline fn open(path: uint8*, attrs: uint32) -> int64 {
    return @asm @clobbers("x0", "x1", "x2", "memory") (SYSCALL_OPEN, path, attrs) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Closes file descriptor fd via SYSCALL_CLOSE (svc #0). Traps into EL1, where
 * syscall_close_handler closes the corresponding fd-table entry.
 *
 * @param fd: file descriptor to close
 *
 * @return 0 on success, -1 on failure.
 */
@inline fn close(fd: int64) -> int64 {
    return @asm @clobbers("x0", "x1", "memory") (SYSCALL_CLOSE, fd) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Reads up to count bytes from fd into buffer via SYSCALL_READ (svc #0). Traps
 * into EL1, where syscall_read_handler dispatches to the process fd table.
 *
 * @param fd:     file descriptor to read from
 * @param buffer: output buffer
 * @param count:  maximum number of bytes to read
 *
 * @return number of bytes read, or a negative error.
 */
@inline fn read(fd: int64, buffer: uint8*, count: uint64) -> int64 {
    return @asm @clobbers("x0", "x1", "x2", "x3", "memory")
        (SYSCALL_READ, fd, buffer, count) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "mov x3, $3"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Writes up to count bytes from buffer to fd via SYSCALL_WRITE (svc #0). Traps
 * into EL1, where syscall_write_handler dispatches to the process fd table.
 *
 * @param fd:     file descriptor to write to
 * @param buffer: input buffer
 * @param count:  number of bytes to write
 *
 * @return number of bytes written, or a negative error.
 */
@inline fn write(fd: int64, buffer: uint8*, count: uint64) -> int64 {
    return @asm @clobbers("x0", "x1", "x2", "x3", "memory")
        (SYSCALL_WRITE, fd, buffer, count) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "mov x3, $3"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Retrieves metadata for fd into stat via SYSCALL_FSTAT (svc #0). Traps into EL1,
 * where syscall_fstat_handler fills the file_stat from the process fd table.
 *
 * @param fd:   file descriptor to stat
 * @param stat: caller-allocated file_stat to fill
 *
 * @return 0 on success, or a negative error.
 */
@inline fn fstat(fd: int64, stat: struct file_stat*) -> int64 {
    return @asm @clobbers("x0", "x1", "x2", "memory")
        (SYSCALL_FSTAT, fd, stat) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "svc #0"
        "mov $out, x0"
    };
}
