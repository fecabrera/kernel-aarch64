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
- **Heap allocator** — first-fit free-list with 4MB region, block splitting, and coalescing (`kmalloc`/`kfree`/`krealloc`)
- **Exception handling** — full save/restore of all 31 registers + ELR/SPSR; IRQ handlers return `struct cpu_context *` for context switching
- **Preemptive scheduler** — round-robin, timer-driven; dedicated 4KB IRQ stack; `create_process`/`destroy_process` with 16-byte aligned task stacks
- **Syscall interface** — `svc #0` dispatch table (`syscall_register_handler`); yield syscall triggers immediate context switch

## Source layout

```
src/
  kernel.c          — kernel_init (subsystem bring-up), kernel_proc (pid 1)
  start.S           — AArch64 boot stub, saves DTB pointer, zeros BSS
  vectors.S         — exception vector table, save/restore_context macros

  arch/             — AArch64-specific
    cpu.c/h         — system register accessors (cntfrq, cntp, DAIF), SPSR defines, halt/hang
    irq.c/h         — exception handlers, IRQ dispatch table, cpu_context, irq_init
    syscall.c/h     — syscall dispatch table, syscall_handler, syscall_register_handler

  drivers/          — MMIO peripheral drivers
    uart.c/h        — PL011 UART
    gic.c/h         — GIC-400 distributor + CPU interface
    timer.c/h       — ARM generic timer
    rtc.c/h         — PL031 RTC

  mm/               — memory subsystem
    mem.c/h         — reads RAM base/size from DTB at boot
    heap.c/h        — kmalloc/kfree/krealloc/kmalloc_aligned

  lib/              — architecture-independent libraries
    dtb.c/h         — FDT parser (be32, node/property walker)
    string.c/h      — freestanding string library (memcpy, memset, strcmp, ...)
    types.h         — stdint-style typedefs

  sched/            — scheduler and process management
    process.c/h     — process struct, create/config/destroy
    scheduler.c/h   — round-robin run queue, scheduler_enqueue, scheduler_handler
```

## Memory map (QEMU virt)

| Address | Device |
|---------|--------|
| `0x40000000` | Kernel load address / RAM base |
| `0x09000000` | PL011 UART0 |
| `0x09010000` | PL031 RTC |
| `0x08000000` | GIC distributor |
| `0x08010000` | GIC CPU interface |
