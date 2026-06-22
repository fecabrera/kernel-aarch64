# AArch64 Bare-Metal Kernel

A minimal bare-metal kernel for AArch64, targeting the QEMU `virt` machine.

## Requirements

- `aarch64-elf-gcc` and `aarch64-elf-ld` (cross-compiler toolchain)
- `qemu-system-aarch64`
- `dosfstools`

On macOS:

```sh
brew install aarch64-elf-gcc qemu dosfstools
```

### Development tools

- `dtc`

On macOS:

```sh
brew install dtc
```

## Build

```sh
make
```

Enable debug logging (activates `dprintk` output):

```sh
make CFLAGS_EXTRA=-DDEBUG
```

Targets:

- `make build` — kernel only (`kernel.elf`, `kernel.img`)
- `make` / `make all` — kernel + `init.img` (100MB FAT32 ramdisk populated from `init/`)

## Run

```sh
make run
```

## mcc port status

The kernel is mid-migration from C to
[**mc**](https://github.com/fecabrera/mcc), a separate language built on LLVM
and cross-compiled to AArch64 by `mcc`. mc buys generics (one `dict<V>` instead
of N copies of `hashmap64`), `defer`-based scope cleanup, and a real module
system with explicit linkage — so the port trades hand-rolled, per-type C
boilerplate for type-safe, reusable code. mc code links against C through
`@extern` bindings, so the migration proceeds subsystem by subsystem and uses
`libmc` (mcc's standard library) in place of the in-tree C utilities.

**Ported to `.mc`** (`src/kernel/`, `src/console.mc`):

- VFS — node tree + mount system (`filesystem/fs.mc`, `filesystem/vfs.mc`)
- FAT32 driver — boot-sector parse, FAT table read, cluster-chain + fs-tree traversal, mount/read (`filesystem/fat32.mc`)
- I/O module registry (`io.mc`)
- Process management (`process.mc`)
- Syscall dispatch table + `svc` wrappers (`syscall.mc`, `system/syscall.mc`; wrappers emit `svc #0` via inline `@asm`)
- Exception/IRQ dispatch — sync/irq/fiq/serr handlers, IRQ registry, vbar_el1 setup (`interrupts/irq.mc`)
- Scheduler — ready/wait/sleep queues, scheduler_handler, spawn/enqueue/dequeue, and all syscall handlers (exit/yield/getpid/waitpid/fork/sleep/msleep) (`scheduler.mc`)
- Drivers — GIC, PL031 RTC, ARM generic timer, PL011 UART, virtio MMIO (`interrupts/gic.mc`, `interrupts/drivers/pl031.mc`, `interrupts/drivers/timer.mc`, `interrupts/drivers/pl011.mc`, `interrupts/drivers/virtio_mmio.mc`)
- Kernel log — `printk`/`dprintk` (`debug.mc`) and the PL011 print path (`pl011_vprintf`), formatting via stb_sprintf bound through `libc/stdio.mc`
- Device handlers — serial, storage (`devices/`)
- Memory management — free-list heap allocator (`heap.mc`: `struct heap` with block split/merge), plus the kernel heap, DTB memory probe, and `kmalloc`/`kfree`/`krealloc`/`kmalloc_aligned` wrappers (`mm.mc`)
- Interactive console / shell (`console.mc`)
- CPU helpers (`cpu.mc`) — register accessors (cntpct/cntfrq/cntp_ctl/cntp_tval, vbar_el1), wfi/wfe, halt/hang, irq enable/disable, and bswap, all via inline `@asm`
- Boot entry / init sequence (`kernel.mc`) — `kernel_init` subsystem bring-up and the `init` (pid 1) entry point

**Still C, bound from mc via `@extern`** (port targets):

- FDT parser — `src/lib/dtb.c` (IRQ-number lookup for timer/RTC/virtio)

**Not being ported:**

- `start.S`, `vectors.S` — boot stub and exception vectors / context save-restore; stay hand-written AArch64 assembly by design.
- C standard library — `src/lib/`; kept as C and bound from mc via `@extern`, not reimplemented in mc.

## Features

### **DTB parsing**

- Reads device tree at boot to discover IRQ numbers, memory layout, and peripheral addresses at runtime.

### **GIC-400**

- Interrupt controller driver with a dynamic dispatch table (`irq_register_handler`).

### **PL011 UART**

- Interrupt-driven RX, polled TX, 115200 8N1.
- `pl011_getc(char *c)` reads the next byte from the RX FIFO without blocking; returns 0 if a byte was read, non-zero if the FIFO is empty.

### **ARM generic timer**

- 10ms tick via EL1 physical timer (PPI 30)
- Tracks `initial_ticks` at boot and `ticks` per interrupt via `cntpct_el0`.
- `timer_get_uptime()` returns milliseconds since boot and is used by the scheduler for absolute-timestamp sleep wakeups.

### **PL031 RTC**

- Match alarm interrupt.

### **virtio MMIO**

- Scans all 32 MMIO slots, validates magic, version, and device ID.
- Negotiates features, sets up per-slot 64-entry virtqueues (`struct virtq` with desc/avail/used rings).
- `virtio_mmio_read` submits synchronous block reads via 3-descriptor chains; polls the used ring via `wfi()` until the device signals completion.
- IRQ handler reads and acks the device's `interrupt_status` register.

### **Filesystem abstraction**

- Generic in-memory tree of `fs_node` structs.
- Each node carries a heap-allocated name, `file_size` (bytes, set from the FAT32 dir entry for files), `attrs` (type + flags), `info` (driver-private pointer; e.g. `fat32_entry_reference *` for FAT32 nodes), `mount` (`struct fs_mount *` of the node's covering mountpoint, or null), and `next`/`child` pointers.
- Node types: `FS_NODE_ATTRS_TYPE_FILE`, `FS_NODE_ATTRS_TYPE_FOLDER`.
- Flag bits: `FS_NODE_ATTRS_FLAG_LINK`, `FS_NODE_ATTRS_FLAG_HIDDEN`, `FS_NODE_ATTRS_FLAG_READONLY`.
- `fs_create_file(name, file_size, attrs, data, mount)` / `fs_create_folder` allocate nodes with an explicit `mount` pointer; folders are initialized with a "." self-reference.
- `fs_get_node_file_size(node)` returns `node->file_size`.
- `fs_add_file_to_folder(parent, name, file_size, attrs, data, mount)` / `fs_add_subfolder` append to a folder's child list and return the new node; both accept a `mount` pointer stored on the new node; `fs_add_file_to_folder` also sets `node->file_size`; `fs_add_subfolder` adds a ".." parent-reference into the new folder.
- `fs_add_to_folder` appends a pre-built node to a folder's child list without type checking.
- `fs_node_rename` replaces a node's heap-allocated name in place.
- `fs_destroy_node` recursively frees a node and its entire child/next subtree.
- `fs_dump_node(node, prefix)` recursively prints a subtree to the kernel log with path prefixes; skips recursing into "." and ".." to avoid cycles.
- `fs_get_child(node, name)` searches a node's direct children by name.
- `fs_remove_child(node, name)` unlinks a named child from a folder's child list without freeing it.
- `fs_read(node, buffer, count, offset)` / `fs_write(node, buffer, count, offset)` dispatch directly through the node's covering `mount` read/write handler. They validate the node is non-null and a file (folders are rejected), returning `FS_IO_ERROR_FILE_NOT_FOUND`, `FS_IO_ERROR_NOT_A_FILE`, `FS_IO_ERROR_MOUNTPOINT_NOT_FOUND`, or `FS_IO_ERROR_HANDLER_NOT_PROVIDED` on failure.
- Folder `.` self-references and `..` parent-references are flagged `FS_NODE_ATTRS_FLAG_LINK | FS_NODE_ATTRS_FLAG_HIDDEN`, so directory listings skip them.
- VFS mount system (`vfs.mc`): `vfs_init` initializes the mount table (a generic `dict`) and creates a global root tree with a "volumes" subfolder; `vfs_create_mountpoint(path, device, info, read, write)` registers a mount entry keyed by path, storing `info` as filesystem-private superblock data, and returns a pointer to the new `fs_mount` (caller creates the VFS node separately; ownership of `info` transferred to the mount); `vfs_destroy_mountpoint(path)` removes the entry, unlinks and destroys the root node.
- `struct fs_mount` carries the mountpoint path, `device` (VFS path of the underlying block device, or null), `info` (filesystem-private superblock data, heap-allocated, freed by `vfs_destroy_mountpoint`), root node pointer, and read/write handlers.
- `vfs_get_mountpoint(path)` looks up a mount entry by its exact path; `vfs_get_node_for_path(path, root)` resolves a path (relative to `root`, or the global root when `root` is null) and returns the `fs_node` directly; `vfs_get_file_size(path)` returns `node->file_size` for the resolved node.
- `vfs_create_dir(path, name, attrs, mount)` / `vfs_create_file(path, name, file_size, attrs, mount)` resolve path and append a new subfolder or file node, storing `mount` in `node->mount`.
- `vfs_read` / `vfs_write` resolve the node and dispatch via `fs_read`/`fs_write` to `node->mount->read`/`write`; return `FS_IO_ERROR_FILE_NOT_FOUND`, `FS_IO_ERROR_NOT_A_FILE`, `FS_IO_ERROR_MOUNTPOINT_NOT_FOUND`, or `FS_IO_ERROR_HANDLER_NOT_PROVIDED` on failure.
- `vfs_dump_fs` prints the entire VFS tree from the global root.

### **FAT32**

- MBR/BPB parsing (`mbr_boot_sector`, `fat32_ext_bs`).
- `fat32_is_boot_sector` validates the 0xAA55 signature and EBR sanity fields.
- `fat32_parse_boot_sector` extracts BPB/EBR fields and computes derived values into `fat32_bs_info`.
- `read_fat_table` (`@private`) reads the full FAT table from disk into `bs_info->fat_table` via `vfs_read`, one sector at a time; entries are 28-bit masked and LE-converted on access (`& 0x0FFFFFFF`) by the cluster-chain walkers.
- `fat32_entry_reference` records a directory entry's on-disk location (cluster number, 8.3 entry index within the sector, and LFN entry count) and is stored in each `fs_node`'s `info` field.
- Directory cluster parsing is handled by `fat32_read_cluster` (`@private`); it handles multi-fragment LFN sequences, falls back to the space-padded 8.3 short name when no LFN is present, populates an `fs_node` tree, and records each entry's node in a `set<uint32, fs_node*>` keyed by data cluster for recursive parent linking.
- `fat32_mount(device_path, mountpoint)` reads the boot sector from `device_path` via `vfs_read`, validates the FAT32 signature, parses the BPB, creates or resolves the VFS directory, registers a mountpoint, reads the full FAT table, then recursively traverses all directories via `fat32_build_fs_tree`, registering `fat32_read`/`fat32_write` as `vfs_handler_t` callbacks. If `mountpoint` is NULL the volume is auto-mounted at `/volumes/<label>`; otherwise the node at the given path is resolved (following link nodes to their target) and must be a folder. The full FAT table is kept alive in `bs_info->fat_table` for cluster chain traversal at read time. Mount failures return the named `FAT32_MOUNT_ERROR_*` codes (`-1` boot-sector read … `-7` fs-tree build).
- `fat32_unmount(device_path)` frees `bs_info->fat_table` and destroys the mountpoint; not yet implemented.
- `fat32_read` re-reads the dir entry from disk to get the file's first cluster and size, walks `bs_info->fat_table` to the cluster containing `offset`, reads the required sectors into a temporary buffer, and copies `count` bytes into `buffer`. `fat32_write` is not yet implemented.
- `fat32_read_cluster` and `fat32_build_fs_tree` (`@private`) traverse the directory cluster chain and populate the `fs_node` tree via `vfs_read`; `fat32_build_fs_tree` resolves each chain's parent through the `set<uint32, fs_node*>` map (falling back to the mount root), and each node is stamped with the `fs_mount *` so dispatch is O(1) at read time.
- `fat32_build_cluster_chains` scans `bs_info->fat_table` from `root_cluster` to `n_fat_entries`, marks visited clusters to avoid duplicates, and pushes the start cluster of each distinct chain into an output `queue<uint32>`.
- `fat32_bs_info` exposes both `total_sectors_16` and `total_sectors_32` (resolved into `total_sectors`) for volumes using the 16-bit sector count field; also carries `fat_table` (heap-allocated, owned by `bs_info`). `bs_info` itself is handed to `vfs_create_mountpoint` as the mount's private `info` and freed by `vfs_destroy_mountpoint`; it is freed directly by `fat32_mount` only on the error paths before ownership transfers.
- 8.3 and LFN directory entry structs (`fat32_dir_entry`, `fat32_lfn_entry`) with `FAT32_ATTR_*`, `FAT32_DIRENT_*`, and `FAT32_LFN_*` defines.
- Packed structs with aligned mirrors for safe field access on AArch64.
- Dump functions for boot sector, EBR, dir, and LFN entries.

### **Heap allocator**

- First-fit free-list with 16 MiB static region, block splitting, and coalescing (`kmalloc`/`kfree`/`krealloc`).
- `kfree` returns error codes for NULL, out-of-range, and double-free.
- `kmalloc_aligned` rounds size up to the alignment boundary.
- Safe for any power-of-two multiple of 8, result is directly `kfree`-able.

### **Exception handling**

- Full save/restore of all 31 registers + ELR/SPSR.
- IRQ handlers return `struct cpu_context *` for context switching.
- IABT/DABT terminate the faulting process via `syscall_exit_handler`.
- Unknown exceptions dump the full register context.

### **Preemptive scheduler**

- FIFO run queue, timer-driven.
- Tracks the running process via a `current` pointer.
- Dedicated 4KB IRQ stack.
- `create_process`/`duplicate_process`/`destroy_process` with 16-byte aligned task stacks.
- Each process tracks a current working directory (`fs_node *`), seeded to the VFS root by `create_process` and inherited from the parent on `duplicate_process` (fork).
- Idles via `halt` when the ready queue is empty.
- Ready queue is a generic `queue<process*>`; the waitpid and sleep queues are generic `set`s keyed by pid.
- Context-switch logging (`dprintk`) is silent unless `DEBUG=1`.

### **IRQ interface**

- IRQ dispatch table backed by a `set64` hash map; `irq_init()` must be called before any handler registration.
- `irq_register_handler(irq, fnc)` registers a handler for a GIC IRQ ID; returns -1 if one is already registered.
- `irq_unregister_handler(irq)` removes the handler; returns -1 if none was registered.
- `sync_handler` decodes ESR_EL1 and dispatches to the syscall handler (SVC64), abort handler (IABT/DABT), or logs unknown exceptions.
- `irq_handler` acknowledges the interrupt via the GIC, dispatches to the registered handler, then signals end-of-interrupt.
- Handlers receive and return `struct cpu_context *`; returning a different pointer triggers a context switch.

### **I/O module registry**

- Named registry of `io_module` structs backed by a `hashmap64`; `io_init()` must be called before any registration. Creates the `/dev` VFS folder and registers it as a mountpoint with `io_read`/`io_write` as handlers, so `vfs_read("/dev/<name>", ...)` dispatches through the I/O registry.
- Each module carries a `uint64_t drv_info` driver-private value and `read`/`write` function pointers (`io_handler_t`: `int (*)(uint8_t *buf, size_t count, size_t offset, uint64_t drv_info)`).
- `io_register_module(name, drv_info, read, write)` allocates and inserts a module and creates a `/dev/<name>` file node in the VFS tree; returns -1 if already registered. `drv_info` is forwarded to the handler on every call.
- `io_unregister_module(name)` removes and frees the module and unlinks the `/dev/<name>` VFS node; returns -1 if not found or the node cannot be removed.
- `io_read(node, buf, n)` / `io_write(node, buf, n)` are `vfs_handler_t` callbacks; they look up the module by `node->name` and dispatch to its handler.

### **Serial console**

- `serial_init()` registers a `/dev/serial` I/O module backed by the PL011 UART. Must be called after `io_init()`.
- `serial_read` blocks reading `count` bytes from the UART by calling `pl011_getc` in a loop, spinning on `wfi()` until each byte arrives; returns `count`.
- `serial_write` writes `count` bytes to the UART one byte at a time via `pl011_putc`; returns `count`.
- `console(pathname)` in `console.mc` opens `pathname` as its stdin/stdout (fds 0/1), then runs an interactive terminal loop, prompting with the current working directory's name (or `/` at the root) followed by `"# "`, tokenizing each input line into `argc/argv` (whitespace-delimited; double-quoted strings are single tokens; backslash escapes any character, including `\"` inside quotes), and dispatching to `console_parse_command`. Lines with an unterminated quote or trailing backslash are rejected with an error.
- `console_parse_command(argc, argv)` forks a child process for each command (parent waits via `waitpid`), except `cd`, which is dispatched directly so it can mutate the console process's own cwd. Built-ins: `ls [path]` (lists a folder's entries relative to the cwd, skipping hidden `.`/`..`; defaults to `/`), `cd <path>` (changes the cwd to a child folder, following `.`/`..` links), `cat <path>` (resolves relative to the cwd, validates the node is a file, then reads and prints it), `echo [args...]` (print first arg), `mount <device> [mountpoint]` (`fat32_mount`; mountpoint defaults to `/volumes/<label>`), `exit [status]` (`exit`), `help` (list commands); unknown commands print a "not found!" message.
- Line input is handled by `libmc/std.mc`'s `readchar()` (blocks on stdin, discarding ANSI escape sequences) and `readline(buffer)` (reads one line via `readchar`, echoing characters and handling backspace and CR; returns character count).

### **Storage devices**

- `storage_init()` scans all virtio MMIO slots for block devices and registers each as an I/O module named `sda`, `sdb`, etc. (alphabetical, in slot order), creating a `/dev/sd<letter>` VFS node. Must be called after `io_init()`.
- `storage_read` reads all 512-byte sectors spanning `[offset, offset+count)` into a temporary buffer via `virtio_mmio_read`, then copies exactly `count` bytes at the correct intra-sector alignment into the caller's buffer; handles non-sector-aligned offsets and multi-sector spans; returns `count` on success or a negative error code from `virtio_mmio_read` on failure.
- `storage_write` is a stub. Both receive the virtio slot index as `slot`.

### **Syscall interface**

- `svc #0` dispatch (`syscall.mc`): `syscall_handler` reads the syscall number from `ctx->x[0]` and dispatches through a table backed by a generic `set<uint64, handler>`; `syscall_init()` must be called before any handler registration. The user-facing `svc` wrappers live in `system/syscall.mc` under bare names (`yield`, `exit`, `fork`, `waitpid`, `sleep`, `time`, `uptime`, `open`, `close`, `read`, `write`, `fstat`, `getcwd`, …) and emit `svc #0` directly via inline `@asm` (each declaring its register/memory `@clobbers`).
- `syscall_register_handler` / `syscall_unregister_handler` add and remove handlers at runtime.
- `yield()` triggers an immediate context switch.
- `exit(status)` terminates the calling process and schedules the next one.
- `getpid()` returns the calling process's PID.
- `waitpid(pid)` blocks the caller until the target process exits.
- `fork()` duplicates the calling process (child resumes at the fork site with return value 0).
- `sleep(seconds)` blocks the caller in a sleep queue; wakes when `timer_get_uptime()` reaches the absolute `sleep_until` timestamp.
- `msleep(ms)` same as `sleep` but duration is in milliseconds.
- `time()` returns the current Unix timestamp from the RTC.
- `uptime()` returns system uptime in milliseconds, computed from `cntpct_el0` ticks since boot.
- `open(path, attrs)` / `close(fd)` / `read(fd, buf, count)` / `write(fd, buf, count)` / `fstat(fd, stat)` operate on the calling process's per-process fd table (see "File descriptors" below).
- `getcwd(buf, size)` writes the calling process's current working directory as an absolute path into `buf`, resolved by walking the `..` chain to the root (`fs_get_absolute_dir`).

### **File descriptors**

- Each process owns an fd table (`process.dtor_ptrs`, a `list<pointer<file_descriptor>*>`); an fd is an index into it. There are no dedicated `stdin`/`stdout` fields — fds 0/1 are stdin/stdout by convention (the root process opens them in that order at startup).
- A `file_descriptor` (`filesystem/file.mc`) is the open-file layer: it binds an `fs_node` to a read/write position (`pos`) and an access mode (`FS_FILE_ATTRS_READ/WRITE/EXEC`). `file_read`/`file_write` check the mode, dispatch to `fs_read`/`fs_write` at `pos`, then advance `pos`; `file_stat` returns size + mode.
- Descriptors are reference-counted via `pointer<file_descriptor>` (`kernel/pointer.mc`): `open` creates one (count 1), `close` clears the slot and `pointer_release`s it, and `fork` `pointer_acquire`s each entry so parent and child share the same descriptor (and `pos`), freed when the last owner closes/exits.
- The `open`/`close`/`read`/`write`/`fstat` syscalls resolve into `process_open/close/read/write_file` / `process_file_stat`, which index the fd table and call the `file_*` layer — completing the loop syscall → `file_*` → `fs_*` → mount handler.
- `printf`/`vprintf` (`libc/stdio.mc`) format with stb_sprintf and stream to `STDOUT_FILENO` via the `write` syscall, so they require the caller to have that fd open. The console opens stdin/stdout on its device at startup (fds 0/1) and forked command children inherit them, so command output flows printf → `write` → fd table → `fs_write` → device.

## Source layout

Implementations live in `.mc` under `src/kernel/`. The remaining C is the
freestanding libc and FDT parser (`src/lib/`), which mc `@extern`-binds. The
per-subsystem C interface headers have been
removed now that every subsystem is mc; what survives under `arch/`/`drivers/`
is only what the remaining C still includes (`cpu.h`'s endian macros,
`virtio_mmio.h`'s register structs), plus the still-vestigial `arch/irq.h` and
`arch/syscall.h`. `src/libmc/` is mcc's standard library; `start.S`/`vectors.S`
stay hand-written assembly and call `kernel_init` (in `kernel.mc`).

```
init/               — files bundled into init.img at build time (FAT32 ramdisk)

src/
  kernel.mc         — kernel_init: subsystem bring-up (DTB, memory, IRQ, VFS, I/O, serial, storage, scheduler, timer); init(): pid 1 entry point, runs console("/dev/serial")
  console.mc        — console()/console_parse_command(): interactive terminal loop with cwd-aware prompt and argc/argv tokenization (quoted strings, backslash escapes), fork-per-command dispatch (cd dispatched directly); built-ins: ls, cd, cat, echo, mount, exit, help; line input via libmc/std readline
  start.S           — AArch64 boot stub, saves DTB pointer, zeros BSS
  vectors.S         — exception vector table, save/restore_context macros

  kernel/           — mc kernel modules (logic + @extern bindings to the C below)
    cpu.mc          — cpu_context, SPSR/CNTP_CTL defines; inline-@asm impls of wfe/wfi, halt/hang, irq_enable/disable, timer registers, set_vbar_el1, bswap; be*/le* helpers
    syscall.mc      — svc #0 dispatch table (generic set): syscall_init/register/unregister_handler, syscall_handler, SYSCALL_* numbers
    dtb.mc          — @extern bindings to the C DTB parser: dtb_init/dump/find_prop, *_irq_number lookups, fdt_header/memreg/fdt_prop structs
    pointer.mc      — generic refcounted pointer<T>: create_pointer/pointer_acquire/pointer_release
    process.mc      — process struct (cwd + refcounted fd table), create/duplicate/set_entry/destroy_process, process_open/close/read/write_file + process_file_stat
    scheduler.mc    — full scheduler: idle ctx+stack, ready/wait/sleep queues, scheduler_init/enqueue/dequeue/spawn/handler, notify_sleepers/waiters, all syscall handlers, scheduler_get/set_current_process
    io.mc           — I/O module registry: io_init, io_register/unregister_module, io_read/io_write
    debug.mc        — printk (always on) / dprintk (DEBUG-gated via @if), thin wrappers over pl011_vprintf
    uchar.mc        — utf16lencpy/utf16bencpy bindings
    devices/
      serial.mc     — /dev/serial driver: serial_init, serial_read (pl011_getc+wfi), serial_write (pl011_putc)
      storage.mc    — block driver: storage_init scans virtio slots, registers /dev/sd<letter>; storage_read/write handlers
    interrupts/     — exception/IRQ dispatch, GIC, and peripheral drivers
      irq.mc        — exception/IRQ dispatch: sync/irq/fiq/serr handlers, ESR_EC decode, generic-set IRQ registry, irq_init (vbar_el1), irq_register/unregister_handler
      gic.mc        — GIC impl: gicd_regs/gicc_regs register structs, gic_init/enable_irq/trigger_sgi/acknowledge/end_of_interrupt
      drivers/      — mc peripheral drivers (full impls)
        pl011.mc    — PL011 UART impl: pl011_regs struct, pl011_init/getc/putc/printf/vprintf, pl011_irq_handler; pl011_vprintf streams to the UART via stb_sprintf's vsprintfcb (pl011_printf_cb callback)
        virtio_mmio.mc — virtio MMIO impl: virtio_mmio_regs/virtq structs, virtio_mmio_init/probe_device/read/find_next_slot, virtio_mmio_irq_handler
        pl031.mc    — PL031 RTC impl: pl031_regs struct, pl031_init/get_time/set_time/set_alarm, pl031_irq_handler, syscall_time_handler
        timer.mc    — ARM generic timer impl: timer_init/get_uptime/set_interval, timer_irq_handler, syscall_uptime_handler
    filesystem/
      file.mc       — open-file layer: file_descriptor (node + pos + access mode), file_stat; file_init/read/write/stat
      fs.mc         — fs_node tree primitives + fs_read/fs_write dispatch; fs_node / fs_mount structs
      vfs.mc        — VFS mount system: vfs_init, vfs_create/destroy_mountpoint, vfs_get_node_for_path, vfs_read/write, vfs_create_dir/file, vfs_dump_fs; owns the global _fs_root tree
      fat32.mc      — full FAT32 implementation: boot-sector parse, FAT-table read, cluster-chain + fs-tree traversal, mount/unmount/read/write; structs/defines come from src/fs/fat32.h
    heap.mc         — free-list heap allocator: heap_init/heap_acquire/heap_release/heap_resize over a struct heap, with block splitting and adjacent-free coalescing
    mm.mc           — kernel memory management: 16 MiB static kernel heap, mem_init (reads RAM base/size from the DTB at boot), kmalloc/kfree/krealloc/kmalloc_aligned wrappers over the kernel heap
    system/
      syscall.mc    — user-facing svc wrappers: yield/exit/getpid/waitpid/fork/sleep/msleep/time/uptime/open/close/read/write/fstat/getcwd, all emitting svc #0 via inline @asm

  libmc/            — mcc standard library (generic, type-parametric)
    memory.mc       — alloc<T>/alloc_aligned<T>/resize<T>/dealloc<T>, copy_bytes/set_bytes/copy_items/set_items
    list.mc         — dynamic list<T> with list_it/list_next cursor (for-in)
    string.mc       — growable byte string (list<uint8> specialization, @inline wrappers)
    dict.mc         — string-keyed dict<V> (open addressing, dict_it/dict_next cursor)
    set.mc, queue.mc, stack.mc — generic containers (set_entry extends pair; set_it/set_next cursor)
    hash.mc         — hash<T> dispatch (splitmix64 for values, fnv1a for pointers)
    std.mc          — ergonomic stdio over the fd syscalls: print/println, writechar/writestr/writeln, readchar/readline
    ascii.mc, uchar.mc
    hashing/        — splitmix64, fnv1a, crc32, murmur3
    iteration/      — pair
    libc/           — freestanding ctype, stdlib, string, limits; stdio binds the stb_sprintf family (vsprintf/snprintf/vsprintfcb) and implements printf/vprintf/getchar/putchar over the read/write syscalls

  arch/             — AArch64 C headers
    cpu.h           — bswap16/32/64 + be*/le* endian macros (used by lib/dtb.c, lib/uchar.c) and halt/hang declarations; register-accessor/halt/hang/wfe/wfi impls are in kernel/cpu.mc
    irq.h, syscall.h — vestigial: cpu_context/irq_handler_t typedefs and SYSCALL_* numbers, no remaining C caller (impls in kernel/interrupts/irq.mc, kernel/syscall.mc, kernel/system/syscall.mc)

  drivers/          — MMIO peripheral C headers
    virtio_mmio.h   — register/virtq structs used by lib/dtb.c for slot discovery; the driver impl is in kernel/interrupts/drivers/virtio_mmio.mc

  lib/              — C standard library
    debug.h         — printk/dprintk prototypes for C callers (impl ported to kernel/debug.mc)
    dtb.c/h         — FDT parser (node/property walker); IRQ number lookup for timer, RTC, and virtio MMIO slots
    stb_sprintf.h   — vendored printf-family formatter; STB_SPRINTF_IMPLEMENTATION is compiled in stdio.c
    stdio.c/h       — pulls in stb_sprintf (vsprintf/snprintf/vsprintfcb)
    string.c/h, ctype.c/h, stdlib.c/h, uchar.c/h
    stdint.h, stdarg.h, stddef.h, stdbool.h, limits.h, time.h, ascii.h, sys/types.h
```

## Memory map (QEMU virt)

| Address      | Device                         |
| ------------ | ------------------------------ |
| `0x40000000` | Kernel load address / RAM base |
| `0x09000000` | PL011 UART0                    |
| `0x09010000` | PL031 RTC                      |
| `0x08000000` | GIC distributor                |
| `0x08010000` | GIC CPU interface              |
| `0x0a000000` | virtio-blk MMIO (init.img)     |
