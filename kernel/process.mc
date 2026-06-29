import "cpu";
import "list";
import "mm";
import "memory";
import "range";
import "pointer";
import "filesystem/fs";
import "filesystem/vfs";
import "filesystem/file";
import "system/proc";
import "elf/elf64";

const MAX_ARGS = 64;

// Magic planted at the bottom 8 bytes of every task stack. The stack grows down
// toward stack[0]; if this value is ever clobbered the stack has (nearly)
// overflowed into the adjacent heap block. See process_check_stack.
const STACK_CANARY: uint64 = 0xDEADC0DECAFEF00D;

enum proc_state: int64 {
    DEAD    = -1,
    CREATED = 0,
    READY   = 1,
    BLOCKED = 2,
}
    
@static let next_pid: proc_pid = 1;

/**
 * Represents a schedulable process. Created by create_process, configured by
 * process_set_entry, and destroyed by destroy_process.
 *
 * @field pid:         (pid_t) unique process identifier, assigned at creation
 * @field state:       current lifecycle state (proc_state::CREATED, proc_state::READY,
 *                     proc_state::BLOCKED, or proc_state::DEAD)
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
    pid: proc_pid;
    state: proc_state;
    image: uint8*;
    image_size: uint64;
    stack: uint8*;
    stack_size: uint64;
    ctx: struct cpu_context*;
    wait_pid: proc_pid;
    sleep_until: uint64;
    cwd: struct fs_node*;
    dtor_ptrs: struct list<struct pointer<struct file_descriptor>*>;
}

/**
 * Allocates a stack and initializes the process struct with a zeroed context
 * frame. The current working directory defaults to the VFS root. The process
 * is left in proc_state::CREATED state; call process_set_entry before enqueueing.
 *
 * @param proc: caller-allocated process struct to initialize
 * @param stack_block_count: number of 16 KiB stack-pool blocks to allocate for
 *                           the task stack
 *
 * @return 0 on success, -1 if stack allocation fails
 */
fn process_create(proc: struct process*, stack_block_count: uint64) -> int32 {
    if (create_stack(proc, stack_block_count) < 0)
        return -1;

    set_stack_canary(proc);
    init_ctx(proc);

    proc->pid = get_next_pid();
    proc->state = proc_state::CREATED;
    proc->image = null;
    proc->image_size = 0;
    proc->cwd = vfs_root();

    list_init(&proc->dtor_ptrs, 10);

    return 0;
}

/**
 * Allocates a new stack for dest and copies src's stack contents and context
 * frame into it, preserving the ctx offset within the stack. Also duplicates
 * src's loaded image (if any) into a freshly allocated buffer. Assigns dest a
 * new PID, sets its state to proc_state::CREATED, and inherits src's current working
 * directory. Call process_set_entry or adjust dest->ctx->x0 before enqueueing.
 *
 * @param dest: caller-allocated process struct to initialize
 * @param src:  process to copy from
 *
 * @return 0 on success, -1 if stack allocation fails, -2 if image duplication
 *         fails
 */
fn process_duplicate(dest: struct process*, src: struct process*) -> int32 {
    if (dup_stack(dest, src) < 0)
        return -1;
    
    if (dup_image(dest, src) < 0) {
        stack_free(dest->stack, dest->stack_size / STACK_POOL_BLK_SIZE);
        return -2;
    }

    set_stack_canary(dest);
    dup_ctx(dest, src);

    dest->pid = get_next_pid();
    dest->state = proc_state::CREATED;
    dest->cwd = src->cwd;

    let dtor_ptrs = &src->dtor_ptrs;
    list_init(&dest->dtor_ptrs, dtor_ptrs->capacity);

    for dtor in dtor_ptrs {
        list_push(&dest->dtor_ptrs, pointer_acquire(dtor));
    }

    return 0;
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
 * Requires proc->state == proc_state::DEAD.
 *
 * @param proc: process to destroy; must be in proc_state::DEAD state
 *
 * @return 0 on success, -1 if proc->state != proc_state::DEAD
 */
fn process_destroy(proc: struct process*) -> int32 {
    if (proc->state != proc_state::DEAD)
        return -1;

    stack_free(proc->stack, proc->stack_size / STACK_POOL_BLK_SIZE);

    proc->stack = null;
    proc->stack_size = 0;
    proc->ctx = null;
    proc->cwd = null;

    destroy_image(proc);

    for dtor in &proc->dtor_ptrs {
        pointer_release(dtor);
    }

    list_destroy(&proc->dtor_ptrs);

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
 * Returns the next free PID and advances the global counter. PIDs are handed out
 * monotonically and never reused.
 */
@private
fn get_next_pid() -> proc_pid {
    let pid = next_pid;
    next_pid = next_pid + 1;
    return pid;
}

/**
 * Core of exec: loads the ELF object at `node` into a fresh process image and
 * redirects proc to it. Reads the file, validates the ELF header, parses it,
 * allocates a new image sized to its memsz (replacing any previous one), applies
 * relocations, locates libcrt's "entry" symbol, then rebuilds proc's stack/context
 * (via setup_args + process_set_entry) so the next scheduling resumes the new
 * program at entry with argc/argv in x0/x1. On success the old image and stack
 * contents are abandoned, execve-style. Shared by process_exec and process_exec_at.
 *
 * @param proc: process to replace the image of
 * @param node: VFS node of the executable to load
 * @param argc: number of arguments to pass
 * @param argv: argument vector; argv[0] is the program name
 *
 * @return 0 on success, or an exec_err (NOT_ENOUGH_MEMORY, IO_ERROR, INVALID_ELF,
 *         PARSING_ERROR, CANNOT_REPLACE_IMAGE, RELOCATION_ERROR, ENTRY_NOT_FOUND)
 */
@private
fn process_exec_node(proc: struct process*, node: struct fs_node*, argc: int64, argv: char**) -> exec_err {
    // allocate buffer
    let size = node->file_size;
    let buf: uint8* = alloc<uint8>(size);
    if (buf == null)
        return exec_err::NOT_ENOUGH_MEMORY;

    // read file
    let rd_status = fs_read(node, buf, size, 0);
    if (rd_status < 0) {
        dprintk("[process] fs_read() returned %lld!\n", rd_status);
        dealloc(buf);
        return exec_err::IO_ERROR;
    }

    // check if it's a valid elf header
    if (!is_elf(buf)) {
        dprintk("[process] \"%s\" is not an ELF file!\n", argv[0]);
        dealloc(buf);
        return exec_err::INVALID_ELF;
    }

    // parse elf file
    let elf_file: struct elf64_file;
    let elf_rd_status = elf_read(buf, size, &elf_file);
    if (elf_rd_status < 0) {
        dprintk("[process] elf_read() returned %u\n", elf_rd_status);
        dealloc(buf);
        return exec_err::PARSING_ERROR;
    }

    // create process image
    let ri_status = replace_image(proc, elf_file.memsz);
    if (ri_status < 0) {
        dprintk("[process] replace_image() returned %lld!\n", ri_status);
        dealloc(buf);
        return exec_err::CANNOT_REPLACE_IMAGE;
    }
    
    // resolve symbols, relocate if necessary, and load sectors to memory
    elf64_load(&elf_file, proc->image as uint64);

    // check for relocation errors
    if (elf_file.relerrs > 0) {
        dprintk("[process] relocation errors: %u\n", elf_file.relerrs);
        dealloc(buf);
        return exec_err::RELOCATION_ERROR;
    }

    dealloc(buf);

    // get entry symbol (libcrt's "entry" wraps main and calls exit on return)
    let entry = elf64_locate_symbol(&elf_file, "entry") as fn ();
    if (entry == null) {
        dprintk("[process] could not find symbol \"entry\"!\n");
        return exec_err::ENTRY_NOT_FOUND;
    }

    // build a fresh context at the top of the existing stack and point it at the
    // new program, execve-style: the old stack contents are abandoned, control
    // resumes at entry in EL1h with argc/argv in x0/x1.
    setup_args(proc, argc, argv);
    process_set_entry(proc, entry);

    return 0;
}

/**
 * Allocates the task stack from the stack pool and records its size on proc.
 *
 * @param proc:              process to allocate a stack for
 * @param stack_block_count: number of 16 KiB pool blocks to allocate
 *
 * @return 0 on success, -1 if the stack pool is exhausted
 */
@private
fn create_stack(proc: struct process*, stack_block_count: uint64) -> int32 {
    proc->stack_size = stack_block_count * STACK_POOL_BLK_SIZE;
    proc->stack = stack_alloc(stack_block_count);
    
    if (proc->stack == null)
        return -1;
    
    return 0;
}

/**
 * Plants the overflow-guard canary at the bottom 8 bytes of proc's stack. See
 * process_check_stack.
 *
 * @param proc: process whose stack to mark
 */
@private
fn set_stack_canary(proc: struct process*) {
    // bottom-of-stack overflow guard
    *(proc->stack as uint64*) = STACK_CANARY;
}

/**
 * Allocates a `size`-byte image buffer from the kernel heap and records it on
 * proc. The image holds a loaded program's code/data; see process_exec.
 *
 * @param proc: process to allocate an image for
 * @param size: image size in bytes
 *
 * @return 0 on success, -1 if the heap is exhausted
 */
@private
fn create_image(proc: struct process*, size: uint64) -> int32 {
    proc->image_size = size;
    proc->image = kalloc<uint8>(size);

    if (proc->image == null)
        return -1;

    return 0;
}

/**
 * Frees proc's current image (if any) and allocates a fresh `size`-byte one.
 * Used by exec to swap in a new program's image.
 *
 * @param proc: process whose image to replace
 * @param size: size of the new image in bytes
 *
 * @return 0 on success, -1 if the heap is exhausted
 */
@private
fn replace_image(proc: struct process*, size: uint64) -> int32 {
    destroy_image(proc);
    return create_image(proc, size);
}

/**
 * Frees proc's image buffer (if allocated) and clears the image/image_size
 * fields. Safe to call when proc has no image.
 *
 * @param proc: process whose image to free
 */
@private
fn destroy_image(proc: struct process*) {
    if (proc->image != null) {
        kdealloc(proc->image);

        proc->image = null;
        proc->image_size = 0;
    }
}

/**
 * Places proc's context frame near the top of its stack and zeroes it. Must be
 * called after create_stack (needs proc->stack and proc->stack_size).
 *
 * @param proc: process whose context to initialize
 */
@private
fn init_ctx(proc: struct process*) {
    // vectors.S save_context uses a 272-byte frame (sizeof(cpu_context)=264 + 8
    // bytes padding for 16-byte SP alignment). Place ctx 272 bytes from the top
    // so that after restore_context's "add sp, sp, #272", sp == stack + stack_size
    // (16-byte aligned).
    proc->ctx = &proc->stack[proc->stack_size - 272] as struct cpu_context*;
    bytezero(proc->ctx, 1);
}

/**
 * Lays argc/argv out at the very top of proc's stack and repositions the context
 * frame just below them, so a freshly exec'd program finds its arguments on a
 * clean stack. Copies each argument string to the top (descending), builds an
 * 8-aligned, null-terminated argv pointer array beneath them, then drops the SP
 * to 16-byte alignment and places the (zeroed) ctx 272 bytes below that. argc and
 * the new argv go in the frame's x0/x1. The args sit above the initial SP, so the
 * program's downward-growing stack never overwrites them. Caps argc at MAX_ARGS.
 *
 * @param proc: process whose stack/context to set up
 * @param argc: number of arguments
 * @param argv: source argument vector (read before the stack is overwritten)
 */
@private
fn setup_args(proc: struct process*, argc: int64, argv: char**) {
    let ptrs: uint64[MAX_ARGS];

    let p: uint64 = (proc->stack as uint64) + proc->stack_size;

    // 1. copy each string to the top, recording its new address
    let r = struct range { end = argc };
    for i in &r {
        let l = strlen(argv[i]) + 1;
        p = p - l;
        bytecopy(p as uint8*, argv[i] as uint8*, l);
        ptrs[i] = p;
    }

    // 2. build the argv[] pointer array (argc + 1, null-terminated), 8-aligned
    p = p & (~7) as uint64;
    p = p - (argc as uint64 + 1) * 8;
    let new_argv = p as uint64*;

    for i in &r {
        new_argv[i] = ptrs[i];
    }
    new_argv[argc] = 0;

    // 3. drop SP to 16-byte alignment and place the ctx frame below it
    let sp = p & (~15) as uint64;
    proc->ctx = (sp - 272) as struct cpu_context*;
    bytezero(proc->ctx, 1);

    proc->ctx->x[0] = argc as uint64;
    proc->ctx->x[1] = new_argv as uint64;
}

/**
 * Points dest->ctx at the same offset within dest's stack that src->ctx sits at
 * within src's stack, so the copied frame lines up. Must be called after
 * dup_stack.
 *
 * @param dest: process receiving the rebased context pointer
 * @param src:  process whose ctx offset is mirrored
 */
@private
fn dup_ctx(dest: struct process*, src: struct process*) {
    let ctx_offset: uint64 = (src->ctx as uint64) - (src->stack as uint64);
    dest->ctx = &dest->stack[ctx_offset] as struct cpu_context*;
}

/**
 * Allocates a new stack the same size as src's and deep-copies its contents
 * (including the saved context frame) into dest.
 *
 * @param dest: process to allocate and populate a stack for
 * @param src:  process whose stack is copied
 *
 * @return 0 on success, -1 if the stack pool is exhausted
 */
@private
fn dup_stack(dest: struct process*, src: struct process*) -> int32 {
    // allocate stack
    dest->stack_size = src->stack_size;
    dest->stack = stack_alloc(dest->stack_size / STACK_POOL_BLK_SIZE);

    if (dest->stack == null)
        return -1;

    // deep copy stack
    bytecopy(dest->stack, src->stack, dest->stack_size);
    
    return 0;
}

/**
 * Deep-copies src's loaded image into a freshly allocated buffer on dest. If src
 * has no image (image == null), dest is left imageless and the call succeeds.
 *
 * @param dest: process to receive the copied image
 * @param src:  process whose image is copied
 *
 * @return 0 on success (including the no-image case), -1 if the heap is exhausted
 */
@private
fn dup_image(dest: struct process*, src: struct process*) -> int32 {
    if (src->image == null) {
        dest->image = null;
        dest->image_size = 0;

        return 0;
    }
    
    // allocate image
    dest->image_size = src->image_size;
    dest->image = kalloc<uint8>(dest->image_size);

    if (dest->image == null)
        return -1;

    // deep copy image
    bytecopy(dest->image, src->image, dest->image_size);

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
fn process_open_file(proc: struct process*, pathname: char*, attrs: uint32) -> int64 {
    dprintk("[process] open_file(%lld, \"%s\", %04X)\n", proc->pid, pathname, attrs);

    let root = pathname[0] == '/' ? vfs_root() : proc->cwd;
    let node = vfs_get_node_for_path(pathname, root);
    if (node == null)
        return io_error::NOT_FOUND;
    
    let dtor_ptrs = &proc->dtor_ptrs;
    let fd = dtor_ptrs->length as int64;

    let dtor = create_pointer<struct file_descriptor>();
    list_push(dtor_ptrs, dtor);

    file_init(dtor->value, node, attrs);

    return fd;
}

fn process_open_file_at(proc: struct process*, dirfd: int64, pathname: char*, attrs: uint32) -> int64 {
    dprintk("[process] open_file_at(%lld, %lld, \"%s\", %04X)\n",
            proc->pid, dirfd, pathname, attrs);

    let dtor_ptrs = &proc->dtor_ptrs;
    let dtor: struct pointer<struct file_descriptor>*;
    if (!list_get(dtor_ptrs, dirfd as uint64, &dtor))
        return io_error::NOT_FOUND;

    let node = vfs_get_node_for_path(pathname, dtor->value->fd_node);
    if (node == null)
        return io_error::NOT_FOUND;

    let new_fd = dtor_ptrs->length as int64;

    let new_dtor = create_pointer<struct file_descriptor>();
    list_push(dtor_ptrs, new_dtor);

    file_init(new_dtor->value, node, attrs);

    return new_fd;
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
        return io_error::INVALID_DESCRIPTOR;

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
        return io_error::INVALID_DESCRIPTOR;

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
        return io_error::INVALID_DESCRIPTOR;

    return file_write(dtor->value, buffer, count);
}

/**
 * Fills st with metadata for descriptor fd via file_stat.
 *
 * @param proc: process owning the fd
 * @param fd:   file descriptor to stat
 * @param st:   caller-allocated file_stat to fill
 *
 * @return 0 on success, FILE_IO_ERROR_INVALID_DESCRIPTOR if fd is out of range
 */
fn process_file_stat(proc: struct process*, fd: int64, st: struct file_stat*) -> int64 {
    dprintk("[process] file_stat(%lld, %lld, %p)\n",
            proc->pid, fd, st);

    let dtor_ptrs = &proc->dtor_ptrs;

    let dtor: struct pointer<struct file_descriptor>*;
    if (!list_get(dtor_ptrs, fd as uint64, &dtor) or dtor == null)
        return io_error::INVALID_DESCRIPTOR;

    return file_stat(dtor->value, st);
}

/**
 * Fills stat with metadata for the file at path, resolved relative to the
 * process's cwd, via file_node_stat. Unlike process_file_stat, no open
 * descriptor is required.
 *
 * @param proc: process whose cwd path is resolved against
 * @param path: path to stat, resolved relative to proc->cwd
 * @param st:   caller-allocated file_stat to fill
 *
 * @return 0 on success, FILE_IO_ERROR_NOT_FOUND if the path does not resolve
 */
fn process_stat(proc: struct process*, path: char*, st: struct file_stat*) -> int64 {
    dprintk("[process] stat(%lld, %s, %p)\n",
            proc->pid, path, st);

    let root = path[0] == '/' ? vfs_root() : proc->cwd;
    let node = vfs_get_node_for_path(path, root);
    if (node == null)
        return io_error::NOT_FOUND;

    return file_node_stat(node, st);
}

/**
 * Like process_stat, but resolves path relative to the directory node behind an
 * open descriptor (dirfd) in proc's fd table rather than the cwd. Lets the
 * console test whether a program exists in a search directory it has open.
 *
 * @param proc:  process whose fd table holds dirfd
 * @param dirfd: descriptor of an open directory to resolve path against
 * @param path:  path to stat, relative to dirfd
 * @param st:    caller-allocated file_stat to fill
 *
 * @return 0 on success, INVALID_DESCRIPTOR if dirfd is not open, NOT_FOUND if the
 *         path does not resolve
 */
fn process_stat_at(proc: struct process*, dirfd: int64, path: char*, st: struct file_stat*) -> int64 {
    dprintk("[process] statat(%lld, %lld, %s, %p)\n",
            proc->pid, dirfd, path, st);

    let dtor_ptrs = &proc->dtor_ptrs;
    let dtor: struct pointer<struct file_descriptor>*;
    if (!list_get(dtor_ptrs, dirfd as uint64, &dtor)) {
        dprintk("[process] invalid fd!\n");
        return io_error::INVALID_DESCRIPTOR;
    }

    let node = vfs_get_node_for_path(path, dtor->value->fd_node);
    if (node == null)
        return io_error::NOT_FOUND;

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

/**
 * Replaces the process's image with the program at `path` (resolved relative to
 * the VFS root if absolute, else proc's cwd), passing it argc/argv. execve-style:
 * on success the old image and stack contents are abandoned and the next
 * scheduling resumes the new program at its entry point, so from the caller's
 * perspective the call does not return.
 *
 * @param proc: process whose image is replaced
 * @param path: VFS path of the executable to load
 * @param argc: number of arguments to pass to the new program
 * @param argv: argument vector; argv[0] is the program name
 *
 * @return does not return on success; an exec_err on failure (FILE_NOT_FOUND if
 *         the path does not resolve, otherwise as process_exec_node)
 */
fn process_exec(proc: struct process*, path: char*, argc: int64,
                argv: char**) -> exec_err {
    let root = path[0] == '/' ? vfs_root() : proc->cwd;
    let node = vfs_get_node_for_path(path, root);
    if (node == null) {
        dprintk("[process] \"%s\" not found!\n", path);
        return exec_err::FILE_NOT_FOUND;
    }

    return process_exec_node(proc, node, argc, argv);
}

/**
 * Like process_exec, but resolves `path` relative to an already-open directory
 * descriptor (dirfd) in proc's fd table rather than the cwd. Lets the console
 * exec a program by re-using the directory it opened to find it.
 *
 * @param proc:  process whose image is replaced
 * @param dirfd: descriptor of an open directory to resolve path against
 * @param path:  path of the executable, relative to dirfd
 * @param argc:  number of arguments to pass to the new program
 * @param argv:  argument vector; argv[0] is the program name
 *
 * @return does not return on success; FILE_NOT_FOUND if dirfd is invalid or the
 *         path does not resolve, otherwise as process_exec_node
 */
fn process_exec_at(proc: struct process*, dirfd: int64, path: char*,
                   argc: int64, argv: char**) -> exec_err {
    let dtor_ptrs = &proc->dtor_ptrs;
    let dtor: struct pointer<struct file_descriptor>*;
    if (!list_get(dtor_ptrs, dirfd as uint64, &dtor)) {
        dprintk("[process] invalid fd!\n");
        return exec_err::FILE_NOT_FOUND;
    }

    let node = vfs_get_node_for_path(path, dtor->value->fd_node);
    if (node == null) {
        dprintk("[process] \"%s\" not found!\n", path);
        return exec_err::FILE_NOT_FOUND;
    }

    return process_exec_node(proc, node, argc, argv);
}
