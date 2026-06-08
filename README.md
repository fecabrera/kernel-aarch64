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

## Features

### **DTB parsing**

- Reads device tree at boot to discover IRQ numbers, memory layout, and peripheral addresses at runtime.

### **GIC-400**

- Interrupt controller driver with a dynamic dispatch table (`irq_register_handler`).

### **PL011 UART**

- Interrupt-driven RX, polled TX, 115200 8N1.
- `pl011_read_input` handles escape sequences for arrow keys (ESC `[` A/B/C/D) and maps CR/DEL/LF to their terminal equivalents.

### **ARM generic timer**

- 10ms tick via EL1 physical timer (PPI 30)
- Tracks `initial_ticks` at boot and `ticks` per interrupt via `cntpct_el0`.
- `timer_get_uptime()` returns milliseconds since boot and is used by the scheduler for absolute-timestamp sleep wakeups.

### **PL031 RTC**

- Match alarm interrupt.

### **virtio MMIO**

- Scans all 32 MMIO slots, validates magic, version, and device ID.
- Negotiates features, sets up per-slot 64-entry virtqueues (`struct virtq` with desc/avail/used rings).
- `virtio_mmio_read` submits synchronous block reads via 3-descriptor chains.
- `virtio_mmio_initialize_fat32_device(slot)` parses the FAT32 filesystem on a block device slot, mounts the root node at `/volumes` via `vfs_mount`, and returns the root `fs_node` tree, or NULL on failure.
- IRQ handler acks `VIRTIO_INTERRUPT_STATUS`.

### **Filesystem abstraction**

- Generic in-memory tree of `fs_node` structs.
- Each node carries a heap-allocated name, `size`, `attrs` (type + flags), `data` (driver-private `uint64_t` — stores a pointer or integer; e.g. `fat32_entry_reference *` for FAT32 nodes), and `next`/`child` pointers.
- Node types: `FS_NODE_ATTRS_TYPE_FILE`, `FS_NODE_ATTRS_TYPE_FOLDER`.
- Flag bits: `FS_NODE_ATTRS_FLAG_LINK`, `FS_NODE_ATTRS_FLAG_HIDDEN`, `FS_NODE_ATTRS_FLAG_READONLY`.
- `fs_create_file` / `fs_create_folder` allocate nodes; folders are initialized with a "." self-reference.
- `fs_add_file_to_folder` / `fs_add_subfolder` append to a folder's child list and return the new node; `fs_add_subfolder` also adds a ".." parent-reference into the new folder.
- `fs_add_to_folder` appends a pre-built node to a folder's child list without type checking.
- `fs_node_rename` replaces a node's heap-allocated name in place.
- `fs_destroy_node` recursively frees a node and its entire child/next subtree.
- `fs_dump_node(node, prefix)` recursively prints a subtree to the kernel log with path prefixes; skips recursing into "." and ".." to avoid cycles.
- `fs_get_child(node, name)` searches a node's direct children by name.
- `fs_remove_child(node, name)` unlinks a named child from a folder's child list without freeing it.
- VFS mount system (`vfs.h`): `vfs_init` initializes the mount table and creates a global root tree with a "volumes" subfolder; `vfs_create_mountpoint(path, read, write, data)` registers a mount entry in a `hashmap64`-backed table (caller creates the VFS node separately); `vfs_destroy_mountpoint(path)` removes the entry, unlinks and destroys the root node.
- `struct vfs_mount` carries the mountpoint path, root node pointer, `vfs_handler_t read`/`write` handlers, and a `void *data` field for driver-private context.
- `vfs_get_mountpoint(path)` looks up a mount entry by its exact path; `vfs_get_mountpoint_for_path(path)` walks path left-to-right and returns the deepest mount whose path is a prefix.
- `vfs_create_dir(path, name, attrs)` / `vfs_create_file(path, name, attrs)` resolve path and append a new subfolder or file node.
- `vfs_read` / `vfs_write` resolve the node, find its covering mount via `vfs_get_mountpoint_for_path`, and dispatch to the mount's `read`/`write` handler; return `VFS_IO_ERROR_FILE_NOT_FOUND`, `VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND`, or `VFS_IO_ERROR_HANDLER_NOT_PROVIDED` on failure.
- `vfs_dump_fs` prints the entire VFS tree from the global root.

### **FAT32**

- MBR/BPB parsing (`mbr_boot_sector`, `fat32_ext_bs`).
- `fat32_is_boot_sector` validates the 0xAA55 signature and EBR sanity fields.
- `fat32_parse_boot_sector` extracts BPB/EBR fields and computes derived values into `fat32_bs_info`.
- `fat32_read_fat_table` copies one FAT sector into a flat `uint32_t` array (28-bit masked, compare against `FAT32_FAT_ENTRY_*`).
- `fat32_entry_reference` records a directory entry's on-disk location (cluster number + offset within the sector) and is stored in each `fs_node`'s `data` field.
- Directory cluster parsing is handled inside the virtio MMIO driver (`_virtio_mmio_fat32_read_cluster`); handles multi-fragment LFN sequences, populates an `fs_node` tree, and records subfolder nodes in a `set64` map for recursive traversal.
- `fat32_cluster_chain` struct pairs a cluster range (`start`/`end`) for FAT chain traversal.
- `fat32_bs_info` exposes both `total_sectors_16` and `total_sectors_32` (resolved into `total_sectors`) for volumes using the 16-bit sector count field.
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
- IABT/DABT terminate the faulting process via `exit_handler`.
- Unknown exceptions dump the full register context.

### **Preemptive scheduler**

- FIFO run queue, timer-driven.
- Tracks the running process via a `current` pointer.
- Dedicated 4KB IRQ stack.
- `create_process`/`duplicate_process`/`destroy_process` with 16-byte aligned task stacks.
- Idles via `halt` when the ready queue is empty.

### **IRQ interface**

- IRQ dispatch table backed by a `set64` hash map; `irq_init()` must be called before any handler registration.
- `irq_register_handler(irq, fnc)` registers a handler for a GIC IRQ ID; returns -1 if one is already registered.
- `irq_unregister_handler(irq)` removes the handler; returns -1 if none was registered.
- `sync_handler` decodes ESR_EL1 and dispatches to the syscall handler (SVC64), abort handler (IABT/DABT), or logs unknown exceptions.
- `irq_handler` acknowledges the interrupt via the GIC, dispatches to the registered handler, then signals end-of-interrupt.
- Handlers receive and return `struct cpu_context *`; returning a different pointer triggers a context switch.

### **I/O module registry**

- Named registry of `io_module` structs backed by a `hashmap64`; `io_init()` must be called before any registration. Creates the `/dev` VFS folder and registers it as a mountpoint with `io_read`/`io_write` as handlers, so `vfs_read("/dev/<name>", ...)` dispatches through the I/O registry.
- Each module carries a `uint64_t drv_info` driver-private value and `read`/`write` function pointers (`io_handler_t`: `int (*)(uint8_t *, size_t, uint64_t)`).
- `io_register_module(name, drv_info, read, write)` allocates and inserts a module and creates a `/dev/<name>` file node in the VFS tree; returns -1 if already registered. `drv_info` is forwarded to the handler on every call.
- `io_unregister_module(name)` removes and frees the module and unlinks the `/dev/<name>` VFS node; returns -1 if not found or the node cannot be removed.
- `io_read(node, buf, n)` / `io_write(node, buf, n)` are `vfs_handler_t` callbacks; they look up the module by `node->name` and dispatch to its handler.

### **Syscall interface**

- `svc #0` dispatch table backed by a `set64` hash map; `syscall_init()` must be called before any handler registration.
- `syscall_register_handler` / `syscall_unregister_handler` add and remove handlers at runtime.
- `syscall_yield()` triggers an immediate context switch.
- `syscall_exit(status)` terminates the calling process and schedules the next one.
- `syscall_getpid()` returns the calling process's PID.
- `syscall_waitpid(pid)` blocks the caller until the target process exits.
- `syscall_fork()` duplicates the calling process (child resumes at the fork site with return value 0).
- `syscall_sleep(seconds)` blocks the caller in a sleep queue; wakes when `timer_get_uptime()` reaches the absolute `sleep_until` timestamp.
- `syscall_msleep(ms)` same as `syscall_sleep` but duration is in milliseconds.
- `syscall_time()` returns the current Unix timestamp from the RTC.
- `syscall_uptime()` returns system uptime in milliseconds, computed from `cntpct_el0` ticks since boot.

## Source layout

```
init/               — files bundled into init.img at build time (FAT32 ramdisk)

src/
  kernel.c          — kernel_init (subsystem bring-up), init (pid 1), child (pid 2, forks to create pid 3)
  start.S           — AArch64 boot stub, saves DTB pointer, zeros BSS
  vectors.S         — exception vector table, save/restore_context macros

  arch/             — AArch64-specific
    cpu.c/h         — system register accessors (cntpct, cntfrq, cntp, DAIF), SPSR defines, halt/hang
    irq.c/h         — exception handlers, IRQ dispatch table, cpu_context, irq_init
    syscall.c/h     — syscall dispatch table (set64-backed), syscall_init, syscall_handler, syscall_register_handler, syscall_unregister_handler, syscall_yield, syscall_exit, syscall_getpid, syscall_waitpid, syscall_fork

  drivers/          — MMIO peripheral drivers
    pl011.c/h       — PL011 UART
    gic.c/h         — GIC-400 distributor + CPU interface
    timer.c/h       — ARM generic timer
    pl031.c/h       — PL031 RTC
    virtio_mmio.c/h — virtio MMIO transport: slot scanning, feature negotiation, virtqueue setup (virtq_desc/virtq_avail/virtq_used), IRQ dispatch, virtio_mmio_initialize_fat32_device

  mm/               — memory subsystem
    mem.c/h         — reads RAM base/size from DTB at boot
    heap.c/h        — kmalloc/kfree/krealloc/kmalloc_aligned

  dsa/              — generic data structures
    queue.c/h           — dynamic ring-buffer FIFO queue of uint64_t (queue64_init/push/pop/peek)
    stack.c/h           — dynamic array-backed LIFO stack of uint64_t (stack64_init/push/pop/peek)
    deque.c/h           — doubly-linked deque of uint64_t (deque64_add/remove/peek left/right, find, find_remove, remove, next)
    hashmap.c/h         — open-addressing hash map: string key → uint64_t value (hashmap64_init/destroy/set/get/has/remove)
    set.c/h             — open-addressing hash map: uint64_t key → uint64_t value (set64_init/destroy/set/get/remove)
    ordered_set.c/h     — BST-based ordered set of uint64_t (ordered_set64_init/destroy/insert/remove/contains/min/max/foreach)

  lib/              — architecture-independent libraries
    debug.c/h       — printk (always on) and dprintk (DEBUG=1 only), both backed by pl011_vprintf
    dtb.c/h         — FDT parser (node/property walker); IRQ number lookup for timer, RTC, and virtio MMIO slots
    string.c/h      — freestanding string library (memcpy, memset, strcmp, strntrimend, ...)
    ctype.c/h       — freestanding character classification and conversion (isalpha, isdigit, isspace, tolower, toupper, ...)
    stdlib.c/h      — itoa, vsprintf, sprintf (freestanding; uses __builtin_va_* instead of <stdarg.h>)
    stdint.h        — stdint-style typedefs (uint8_t … uint64_t, intptr_t)
    uchar.c/h       — char8_t/16_t/32_t typedefs; utf16to8 conversion; utf16lencpy/utf16bencpy (UTF-16LE/BE to ASCII)
    limits.h        — integer limit macros (AArch64/LP64; unsigned char default)
    time.h          — time_t typedef (uint64_t)
    sys/types.h     — pid_t typedef (int64_t)

  sched/            — scheduler and process management
    process.c/h     — process struct, create/duplicate/config/destroy
    scheduler.c/h   — FIFO ready queue (dsa/queue64) and wait queue (dsa/deque64), scheduler_enqueue/dequeue/spawn, context switch via timer and yield/exit/waitpid/fork syscalls

  fs/               — filesystem drivers
    filesystem.c/h  — fs_node tree primitives: fs_get_child, fs_remove_child, fs_create_node/file/folder, fs_add_file_to_folder, fs_add_subfolder, fs_add_to_folder, fs_node_rename, fs_destroy_node, fs_dump_node
    vfs.c/h         — VFS mount system: vfs_init, vfs_create_mountpoint, vfs_destroy_mountpoint, vfs_get_node, vfs_dump_fs; owns the global _fs_root tree
    fat32.c/h       — MBR/BPB structs (packed + aligned mirrors), fat32_bs_info, fat32_entry_reference, fat32_is_boot_sector, fat32_parse_boot_sector, fat32_read_fat_table, fat32_build_cluster_chains, 8.3 and LFN dir entry structs, partition type/media descriptor/attribute/LFN defines, dump functions

  io/               — I/O module registry
    module.c/h      — hashmap64-backed named module registry: io_init, io_register_module, io_unregister_module, io_read, io_write; io_module carries attrs + read/write handlers; io_file pairs a pid with a module
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
