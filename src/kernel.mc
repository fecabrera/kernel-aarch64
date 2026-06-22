import "debug";
import "dtb";
import "mm/heap";
import "mm/mem";
import "syscall";
import "interrupts/gic";
import "interrupts/irq";
import "interrupts/drivers/virtio_mmio";
import "interrupts/drivers/pl011";
import "interrupts/drivers/pl031";
import "interrupts/drivers/timer";
import "scheduler";
import "filesystem/fs";
import "filesystem/vfs";
import "io";
import "devices/serial";
import "devices/storage";
import "system/syscall";
import "console";

fn kernel_init() {
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
    let pid = scheduler_spawn(init);
    dprintk("[kernel] spawned init process with pid %lld\n", pid);

    // start scheduler
    timer_init();

    // force context switch via syscall
    yield();
}

fn init() {
    console("/dev/serial");
}
