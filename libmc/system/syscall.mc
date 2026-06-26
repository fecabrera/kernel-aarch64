import "system/fs";

const NUM_SYSCALLS = 256;

enum syscall: uint64 {
    EXIT    = 0,
    YIELD   = 1,
    GETPID  = 2,
    WAITPID = 3,
    FORK    = 4,
    SLEEP   = 5,
    MSLEEP  = 6,
    TIME    = 7,
    UPTIME  = 8,
    OPEN    = 9,
    OPENAT  = 10,
    CLOSE   = 11,
    READ    = 12,
    WRITE   = 13,
    FSTAT   = 14,
    STAT    = 15,
    GETCWD  = 16,
    ACQMEM  = 17,
    RSZMEM  = 18,
    RELMEM  = 19,
}

/**
 * Voluntarily yields the CPU to the scheduler via SYSCALL_YIELD (svc #0).
 * Traps into EL1, where syscall_yield_handler sets the return value to 0 in ctx->x0 and calls
 * scheduler_handler to pick the next runnable task. Execution resumes in the calling task when it
 * is next scheduled.
 *
 * @return 0 on return from the scheduler.
 */
@inline fn yield() -> int64 {
    return @asm @clobbers("x0", "memory") (syscall::YIELD) -> int64 {
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
    @asm @clobbers("x0", "x1", "memory") (syscall::EXIT, status) {
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
    return @asm @clobbers("x0") (syscall::GETPID) -> int64 {
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
    return @asm @clobbers("x0", "x1", "memory") (syscall::WAITPID, pid) -> int64 {
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
    return @asm @clobbers("x0", "memory") (syscall::FORK) -> int64 {
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
    return @asm @clobbers("x0", "x1", "memory") (syscall::SLEEP, seconds) -> int64 {
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
    return @asm @clobbers("x0", "x1", "memory") (syscall::MSLEEP, mseconds) -> int64 {
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
    return @asm @clobbers("x0", "memory") (syscall::TIME) -> uint64 {
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
    return @asm @clobbers("x0", "memory") (syscall::UPTIME) -> uint64 {
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
    return @asm @clobbers("x0", "x1", "x2", "memory") (syscall::OPEN, path, attrs) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "svc #0"
        "mov $out, x0"
    };
}

@inline fn openat(dirfd: int64, path: uint8*, attrs: uint32) -> int64 {
    return @asm @clobbers("x0", "x1", "x2", "x3", "memory")
        (syscall::OPENAT, dirfd, path, attrs) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "mov x3, $3"
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
    return @asm @clobbers("x0", "x1", "memory") (syscall::CLOSE, fd) -> int64 {
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
        (syscall::READ, fd, buffer, count) -> int64 {
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
        (syscall::WRITE, fd, buffer, count) -> int64 {
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
        (syscall::FSTAT, fd, stat) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Retrieves metadata for the file at path into stat via SYSCALL_STAT (svc #0).
 * Traps into EL1, where syscall_stat_handler resolves path relative to the
 * process cwd and fills the file_stat. Unlike fstat, no open descriptor is
 * required.
 *
 * @param path: null-terminated path to stat, resolved relative to the cwd
 * @param stat: caller-allocated file_stat to fill
 *
 * @return 0 on success, or a negative error.
 */
@inline fn stat(path: uint8*, stat: struct file_stat*) -> int64 {
    return @asm @clobbers("x0", "x1", "x2", "memory")
        (syscall::STAT, path, stat) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "svc #0"
        "mov $out, x0"
    };
}

/**
 * Writes the absolute path of the calling process's current working directory
 * into buf via SYSCALL_GETCWD (svc #0). Traps into EL1, where
 * syscall_getcwd_handler resolves the path from the process cwd.
 *
 * @param buf:  output buffer for the null-terminated path
 * @param size: capacity of buf in bytes
 *
 * @return the path length excluding the null terminator, or -1 on failure.
 */
@inline fn getcwd(buf: uint8*, size: uint64) -> int64 {
    return @asm @clobbers("x0", "x1", "x2", "memory")
        (syscall::GETCWD, buf, size) -> int64 {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "svc #0"
        "mov $out, x0"
    };
}

@inline fn acqmem(size: uint64, align: uint64) -> uint8* {
    return @asm @clobbers("x0", "x1", "x2", "memory")
        (syscall::ACQMEM, size, align) -> uint8* {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "svc #0"
        "mov $out, x0"
    };
}

@inline fn rszmem(ptr: uint8*, size: uint64, align: uint64) -> uint8* {
    return @asm @clobbers("x0", "x1", "x2", "memory")
        (syscall::RSZMEM, ptr, size, align) -> uint8* {
        "mov x0, $0"
        "mov x1, $1"
        "mov x2, $2"
        "mov x3, $3"
        "svc #0"
        "mov $out, x0"
    };
}

@inline fn relmem(ptr: uint8*) {
    @asm @clobbers("x0", "x1", "memory")
        (syscall::RELMEM, ptr) {
        "mov x0, $0"
        "mov x1, $1"
        "svc #0"
    };
}
