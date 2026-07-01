import "debug";
import "dtb";
import "syscall";
import "mm";
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
    // Initialize kernel heap
    kheap_init();
    
    // Initialize DTB
    dtb_init();

    // Initialize memory
    mem_init();

    // Initialize syscall system
    syscall_init();

    // Register syscalls for heap memory acquisition, resizing and release
    register_mem_syscalls();
    
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
    let storage_dev = "/dev/sda";
    let io_dev = "/dev/serial";

    let status: int64;
    
    dprintk("[init] opening stdin at \"%s\"...\n", io_dev);
    status = open(io_dev, open_mode::READ);
    if (status < 0) {
        printk("[init] open() returned %lld\n", status);
        hang();
    }

    dprintk("[init] opening stdout at \"%s\"...\n", io_dev);
    status = open(io_dev, open_mode::WRITE);
    if (status < 0) {
        printk("[init] open() returned %lld\n", status);
        hang();
    }

    dprintk("[init] mounting \"%s\"...\n", storage_dev);
    status = fat32_mount(storage_dev, "/");
    if (status < 0) {
        printk("[init] fat32_mount() returned %lld!\n", status);
        hang();
    }
    
    printk("[init] starting console...\n");
    console();
}
