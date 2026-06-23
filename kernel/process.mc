import "cpu";
import "list";
import "memory";
import "pointer";
import "filesystem/fs";
import "filesystem/vfs";
import "filesystem/file";

const DEFAULT_STACK_SIZE = (1 << 14); // 16 KiB

// Magic planted at the bottom 8 bytes of every task stack. The stack grows down
// toward stack[0]; if this value is ever clobbered the stack has (nearly)
// overflowed into the adjacent heap block. See process_check_stack.
const STACK_CANARY: uint64 = 0xDEADC0DECAFEF00D;

const PROC_DEAD = -1;
const PROC_CREATED = 0;
const PROC_READY = 1;
const PROC_BLOCKED = 2;

@static let next_pid: int64 = 1;

/**
 * Represents a schedulable process. Created by create_process, configured by
 * process_set_entry, and destroyed by destroy_process.
 *
 * @field pid:         (pid_t) unique process identifier, assigned at creation
 * @field state:       current lifecycle state (PROC_CREATED, PROC_READY, PROC_BLOCKED, or
 *                     PROC_DEAD)
 * @field stack:       base address of the heap-allocated task stack
 * @field ctx:         saved register frame (cpu_context), embedded near the top of stack and
 *                     updated on every context switch
 * @field stack_size:  size of the stack allocation in bytes
 * @field wait_pid:    (pid_t) PID this process is blocked waiting for via waitpid; 0 when not
 *                     waiting
 * @field sleep_until: (time_t) absolute uptime in milliseconds at which the process should wake; 0
 *                     when not sleeping
 * @field cwd:         current working directory node; inherited from the parent on fork
 * @field dtor_ptrs:   per-process file-descriptor table; fd numbers index this array of
 *                     refcounted pointer<file_descriptor> (shared with children on fork)
 */
struct process {
    pid: int64;
    state: int64;
    stack: uint8*;
    ctx: struct cpu_context*;
    stack_size: uint64;
    wait_pid: int64;
    sleep_until: uint64;
    cwd: struct fs_node*;
    dtor_ptrs: struct list<struct pointer<struct file_descriptor>*>;
}

/**
 * Allocates a stack and initializes the process struct with a zeroed context
 * frame. The current working directory defaults to the VFS root. The process
 * is left in PROC_CREATED state; call process_set_entry before enqueueing.
 *
 * @param proc: caller-allocated process struct to initialize
 * @param stack_size: size in bytes of the task stack to allocate
 *
 * @return 0 on success, -1 if stack allocation fails
 */
fn create_process(proc: struct process*, stack_size: uint64) -> int32 {
    let stack: uint8* = alloc_aligned<uint8>(stack_size, 16);

    if (stack == null) {
        return -1;
    }

    *(stack as uint64*) = STACK_CANARY; // bottom-of-stack overflow guard

    // vectors.S save_context uses a 272-byte frame (sizeof(cpu_context)=264 + 8
    // bytes padding for 16-byte SP alignment). Place ctx 272 bytes from the top
    // so that after restore_context's "add sp, sp, #272", sp == stack + stack_size
    // (16-byte aligned).
    let ctx = (stack as uint64 + stack_size - 272) as struct cpu_context*;
    set_bytes(ctx, 0, 1);

    proc->pid = next_pid;
    proc->state = PROC_CREATED;
    proc->ctx = ctx;
    proc->stack = stack;
    proc->stack_size = stack_size;
    proc->cwd = vfs_root();

    list_init(&proc->dtor_ptrs, 10);

    next_pid = next_pid + 1;

    return 0;
}

/**
 * Allocates a new stack for dest and copies src's stack contents and context
 * frame into it, preserving the ctx offset within the stack. Assigns dest a
 * new PID, sets its state to PROC_CREATED, and inherits src's current working
 * directory. Call process_set_entry or adjust dest->ctx->x0 before enqueueing.
 *
 * @param dest: caller-allocated process struct to initialize
 * @param src:  process to copy from
 *
 * @return 0 on success, -1 if stack allocation fails
 */
fn duplicate_process(dest: struct process*, src: struct process*) -> int32 {
    let stack_size = src->stack_size;
    let stack: uint8* = alloc_aligned<uint8>(stack_size, 16);

    if (stack == null)
        return -1;

    *(stack as uint64*) = STACK_CANARY; // bottom-of-stack overflow guard

    let ctx_offset: uint64 = (src->ctx as uint64) - (src->stack as uint64);
    let ctx = (stack as uint64 + ctx_offset) as struct cpu_context*;
    bytecopy(ctx, src->ctx, 1);

    dest->pid = next_pid;
    dest->state = PROC_CREATED;
    dest->ctx = ctx;
    dest->stack = stack;
    dest->stack_size = stack_size;
    dest->cwd = src->cwd;

    let dtor_ptrs = &src->dtor_ptrs;
    list_init(&dest->dtor_ptrs, dtor_ptrs->capacity);
    for dtor in dtor_ptrs {
        list_append(&dest->dtor_ptrs, pointer_acquire(dtor));
    }

    next_pid = next_pid + 1;

    return 0;
}

/**
 * Configures the process entry point and initial SPSR.
 * Must be called before the process is enqueued in the scheduler.
 *
 * @param proc:  process to configure
 * @param entry: function the process will execute after its first eret
 */
fn process_set_entry(proc: struct process*, entry: fn ()) {
    let ctx: struct cpu_context* = proc->ctx;
    ctx->elr = entry as uint64;
    ctx->spsr = SPSR_EL1h;
}

/**
 * Verifies the bottom-of-stack canary planted at process creation is intact.
 * A clobbered canary means the stack has grown down past its allocation and
 * (nearly) overflowed into the adjacent heap block. Halts on detection so the
 * fault is attributed to the offending process rather than surfacing later as
 * mysterious heap corruption. Debug-only; compiles out of normal builds.
 *
 * @param proc: process whose stack to validate
 */
fn process_check_stack(proc: struct process*) {
    @if (DEBUG) {
        if (proc != null and *(proc->stack as uint64*) != STACK_CANARY) {
            printk("[process] stack overflow detected in pid %lld (stack=%p)!\n",
                   proc->pid, proc->stack);
            halt();
        }
    }
}

/**
 * Frees the task stack and nulls out stack and ctx pointers.
 * Requires proc->state == PROC_DEAD.
 *
 * @param proc: process to destroy; must be in PROC_DEAD state
 *
 * @return 0 on success, -1 if proc->state != PROC_DEAD
 */
fn destroy_process(proc: struct process*) -> int32 {
    if (proc->state != PROC_DEAD)
        return -1;

    dealloc(proc->stack);

    proc->stack = null;
    proc->ctx = null;
    proc->cwd = null;

    for dtor in &proc->dtor_ptrs {
        pointer_release(dtor);
    }

    list_destroy(&proc->dtor_ptrs);

    return 0;
}

/**
 * Opens pathname (resolved relative to the process's cwd) as a new file in the
 * process's fd table. Allocates a refcounted pointer<file_descriptor>, initializes
 * its value via file_init, appends it to proc->dtor_ptrs, and returns its index
 * as the file descriptor.
 *
 * @param proc:     process whose fd table the file is added to
 * @param pathname: path to open, resolved relative to proc->cwd
 * @param attrs:    access mode (FS_FILE_ATTRS_READ/WRITE/EXEC bits)
 *
 * @return the new file descriptor (index into the fd table),
 *         FILE_IO_ERROR_NOT_FOUND if the path does not resolve, or
 *         FILE_IO_ERROR_NOT_A_FILE if it names a folder
 */
fn process_open_file(proc: struct process*, pathname: uint8*, attrs: uint32) -> int64 {
    dprintk("[process] open_file(%lld, \"%s\", %04X)\n", proc->pid, pathname, attrs);

    let node = vfs_get_node_for_path(pathname, proc->cwd);
    if (node == null)
        return FILE_IO_ERROR_NOT_FOUND;
    
    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) == FS_NODE_ATTRS_TYPE_FOLDER) {
        return FILE_IO_ERROR_NOT_A_FILE;
    }

    let dtor_ptrs = &proc->dtor_ptrs;
    let fd = dtor_ptrs->length as int64;

    let dtor = create_pointer<struct file_descriptor>();
    list_append(dtor_ptrs, dtor);

    file_init(dtor->value, node, attrs);

    return fd;
}

/**
 * Closes the file at descriptor fd: clears its fd-table slot and drops the
 * descriptor's reference via pointer_release (freeing it once no other process
 * shares it).
 *
 * @param proc: process owning the fd
 * @param fd:   file descriptor to close
 *
 * @return 0 on success, FILE_IO_ERROR_INVALID_DESCRIPTOR if fd is out of range
 */
fn process_close_file(proc: struct process*, fd: int64) -> int64 {
    dprintk("[process] close_file(%lld, %lld)\n", proc->pid, fd);

    let dtor_ptrs = &proc->dtor_ptrs;

    let dtor: struct pointer<struct file_descriptor>*;
    if (!list_get(dtor_ptrs, fd as uint64, &dtor))
        return FILE_IO_ERROR_INVALID_DESCRIPTOR;

    list_set(dtor_ptrs, fd as uint64, null);
    pointer_release(dtor);

    return 0;
}

/**
 * Reads up to count bytes from descriptor fd into buffer via file_read.
 *
 * @param proc:   process owning the fd
 * @param fd:     file descriptor to read from
 * @param buffer: output buffer
 * @param count:  maximum number of bytes to read
 *
 * @return number of bytes read, -1 if fd is out of range, or a negative error
 *         from file_read
 */
fn process_read_file(proc: struct process*, fd: int64, buffer: uint8*, count: uint64) -> int64 {
    dprintk("[process] read_file(%lld, %lld, %p, %lld)\n",
            proc->pid, fd, buffer, count);

    let dtor_ptrs = &proc->dtor_ptrs;

    let dtor: struct pointer<struct file_descriptor>*;
    if (!list_get(dtor_ptrs, fd as uint64, &dtor) or dtor == null)
        return FILE_IO_ERROR_INVALID_DESCRIPTOR;

    return file_read(dtor->value, buffer, count);
}

/**
 * Writes up to count bytes from buffer to descriptor fd via file_write.
 *
 * @param proc:   process owning the fd
 * @param fd:     file descriptor to write to
 * @param buffer: input buffer
 * @param count:  number of bytes to write
 *
 * @return number of bytes written, -1 if fd is out of range, or a negative error
 *         from file_write
 */
fn process_write_file(proc: struct process*, fd: int64, buffer: uint8*, count: uint64) -> int64 {
    dprintk("[process] write_file(%lld, %lld, %p, %lld)\n",
            proc->pid, fd, buffer, count);

    let dtor_ptrs = &proc->dtor_ptrs;

    let dtor: struct pointer<struct file_descriptor>*;
    if (!list_get(dtor_ptrs, fd as uint64, &dtor) or dtor == null)
        return FILE_IO_ERROR_INVALID_DESCRIPTOR;

    return file_write(dtor->value, buffer, count);
}

/**
 * Fills stat with metadata for descriptor fd via file_stat.
 *
 * @param proc: process owning the fd
 * @param fd:   file descriptor to stat
 * @param stat: caller-allocated file_stat to fill
 *
 * @return 0 on success, FILE_IO_ERROR_INVALID_DESCRIPTOR if fd is out of range
 */
fn process_file_stat(proc: struct process*, fd: int64, st: struct file_stat*) -> int64 {
    dprintk("[process] file_stat(%lld, %lld, %p)\n",
            proc->pid, fd, st);

    let dtor_ptrs = &proc->dtor_ptrs;

    let dtor: struct pointer<struct file_descriptor>*;
    if (!list_get(dtor_ptrs, fd as uint64, &dtor) or dtor == null)
        return FILE_IO_ERROR_INVALID_DESCRIPTOR;

    return file_stat(dtor->value, st);
}

fn process_stat(proc: struct process*, path: uint8*, st: struct file_stat*) -> int64 {
    dprintk("[process] stat(%lld, %s, %p)\0",
            proc->pid, path, st);

    let node = vfs_get_node_for_path(path, proc->cwd);
    if (node == null)
        return FILE_IO_ERROR_NOT_FOUND;

    return file_node_stat(node, st);
}

/**
 * Writes the absolute path of the process's current working directory into buf
 * via fs_get_absolute_dir.
 *
 * @param proc: process whose cwd is resolved
 * @param buf:  output buffer for the null-terminated path
 * @param size: capacity of buf in bytes
 *
 * @return the path length excluding the null terminator, or -1 if it would not
 *         fit in size bytes
 */
fn process_get_cwd(proc: struct process*, buf: uint8*, size: uint64) -> int64 {
    dprintk("[process] get_cwd(%lld, %p, %llu)\n",
            proc->pid, buf, size);

    return fs_get_absolute_dir(proc->cwd, buf, size);
}
