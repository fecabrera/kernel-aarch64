#include "kernel.h"
#include "console.h"
#include <arch/syscall.h>
#include <debug.h>
#include <devices/serial.h>
#include <devices/storage.h>
#include <drivers/gic.h>
#include <drivers/pl011.h>
#include <drivers/pl031.h>
#include <drivers/timer.h>
#include <drivers/virtio_mmio.h>
#include <dtb.h>
#include <fs/vfs.h>
#include <io/module.h>
#include <mm/heap.h>
#include <mm/mem.h>
#include <sched/scheduler.h>

void kernel_init() {
    // Initialize DTB
    dtb_init();
    dtb_dump();

    // Initialize memory
    mem_init();
    heap_init();
    heap_dump();

    // Initialize syscall system
    syscall_init();

    // Initialize interrupts
    gic_init();
    irq_init();
    virtio_mmio_init();
    pl011_init();
    pl031_init();
    scheduler_init();
    irq_enable();

    // initialize file system and I/O
    vfs_init();
    io_init();
    serial_init();
    storage_init();

    // set up root process
    pid_t pid = scheduler_spawn(&init);
    dprintk("[kernel] spawned init process with pid %i\r\n", pid);

    // start scheduler
    timer_init();

    // force context switch via syscall
    syscall_yield();
}

void init() { console("/dev/serial"); }