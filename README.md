# AArch64 Bare-Metal Kernel

A minimal bare-metal kernel for AArch64, targeting the QEMU `virt` machine.

## Requirements

- `aarch64-elf-gcc` and `aarch64-elf-ld` (cross-compiler toolchain)
- `qemu-system-aarch64`

On macOS:

```sh
brew install aarch64-elf-gcc qemu
```

## Build

```sh
make
```

Output: `kernel.elf` and `kernel.img`.

## Run

```sh
make run
```

## Features

- **DTB parsing** — reads device tree at boot to discover IRQ numbers, memory layout, and peripheral addresses at runtime
- **GIC-400** — interrupt controller driver with a dynamic dispatch table (`irq_register_handler`)
- **PL011 UART** — interrupt-driven RX, polled TX, 115200 8N1
- **ARM generic timer** — 10ms tick via EL1 physical timer (PPI 30)
- **PL031 RTC** — match alarm interrupt
- **Heap allocator** — first-fit free-list with 4MB region, block splitting, and coalescing (`kmalloc`/`kfree`/`krealloc`); `kfree` returns error codes for NULL, out-of-range, and double-free
- **Exception handling** — full save/restore of all 31 registers + ELR/SPSR; IRQ handlers return `struct cpu_context *` for context switching; IABT/DABT terminate the faulting process via `exit_handler`; unknown exceptions dump the full register context
- **Preemptive scheduler** — FIFO run queue, timer-driven; tracks the running process via a `current` pointer; dedicated 4KB IRQ stack; `create_process`/`duplicate_process`/`destroy_process` with 16-byte aligned task stacks
- **Syscall interface** — `svc #0` dispatch table (`syscall_register_handler`); `syscall_yield()` triggers an immediate context switch; `syscall_exit(status)` terminates the calling process and schedules the next one; `syscall_getpid()` returns the calling process's PID; `syscall_waitpid(pid)` blocks the caller until the target process exits; `syscall_fork()` duplicates the calling process (child resumes at the fork site with return value 0); `syscall_sleep(seconds)` blocks the caller in a sleep queue; the timer tick decrements `sleep_for` until it reaches zero; `syscall_time()` returns the current Unix timestamp from the RTC

## Source layout

```
src/
  kernel.c          — kernel_init (subsystem bring-up), init (pid 1), child (pid 2, forks to create pid 3)
  start.S           — AArch64 boot stub, saves DTB pointer, zeros BSS
  vectors.S         — exception vector table, save/restore_context macros

  arch/             — AArch64-specific
    cpu.c/h         — system register accessors (cntfrq, cntp, DAIF), SPSR defines, halt/hang
    irq.c/h         — exception handlers, IRQ dispatch table, cpu_context, irq_init
    syscall.c/h     — syscall dispatch table, syscall_handler, syscall_register_handler, syscall_yield, syscall_exit, syscall_getpid, syscall_waitpid, syscall_fork

  drivers/          — MMIO peripheral drivers
    uart.c/h        — PL011 UART
    gic.c/h         — GIC-400 distributor + CPU interface
    timer.c/h       — ARM generic timer
    rtc.c/h         — PL031 RTC

  mm/               — memory subsystem
    mem.c/h         — reads RAM base/size from DTB at boot
    heap.c/h        — kmalloc/kfree/krealloc/kmalloc_aligned

  dsa/              — generic data structures
    queue.c/h       — dynamic ring-buffer FIFO queue of uint64_t (queue64_init/push/pop/peek)
    stack.c/h       — dynamic array-backed LIFO stack of uint64_t (stack64_init/push/pop/peek)
    deque.c/h       — doubly-linked deque of uint64_t (deque64_add/remove/peek left/right, find, find_remove, remove, next)

  lib/              — architecture-independent libraries
    dtb.c/h         — FDT parser (be32, node/property walker)
    string.c/h      — freestanding string library (memcpy, memset, strcmp, ...)
    stdlib.c/h      — itoa, vsprintf, sprintf (freestanding; uses __builtin_va_* instead of <stdarg.h>)
    stdint.h        — stdint-style typedefs (uint8_t … uint64_t, intptr_t)
    limits.h        — integer limit macros (AArch64/LP64; unsigned char default)
    time.h          — time_t typedef (uint64_t)
    sys/types.h     — pid_t typedef (int64_t)

  sched/            — scheduler and process management
    process.c/h     — process struct, create/duplicate/config/destroy
    scheduler.c/h   — FIFO ready queue (dsa/queue64) and wait queue (dsa/deque64), scheduler_enqueue/dequeue, context switch via timer and yield/exit/waitpid/fork syscalls
```

## Memory map (QEMU virt)

| Address      | Device                         |
| ------------ | ------------------------------ |
| `0x40000000` | Kernel load address / RAM base |
| `0x09000000` | PL011 UART0                    |
| `0x09010000` | PL031 RTC                      |
| `0x08000000` | GIC distributor                |
| `0x08010000` | GIC CPU interface              |
