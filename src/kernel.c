#include <dtb.h>
#include <debug.h>
#include <arch/cpu.h>
#include <arch/irq.h>
#include <arch/syscall.h>
#include <drivers/pl011.h>
#include <drivers/gic.h>
#include <drivers/timer.h>
#include <drivers/pl031.h>
#include <drivers/virtio_mmio.h>
#include <mm/heap.h>
#include <mm/mem.h>
#include <sched/process.h>
#include <sched/scheduler.h>
#include <fs/fat32.h>
#include <fs/filesystem.h>
#include <fs/vfs.h>
#include <io/module.h>
#include <devices/storage.h>
#include "kernel.h"

void kernel_init()
{
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
    storage_init();

    // set up root process
    pid_t pid = scheduler_spawn(&init);
    printk("[kernel] spawned init process with pid %i\r\n", pid);

    // start scheduler
    timer_init();

    // force context switch via syscall
    syscall_yield();
}

void init()
{
    pid_t pid = syscall_getpid();

    printk("[init] pid = %i\r\n", pid);

    virtio_slot_t slot = -1;
    while ((slot = virtio_mmio_find_next_slot(VIRTIO_DEVICE_ID_BLOCK, slot)) != -1)
    {
        // build path, will eventually replace with a direct fetch
        char path[50] = {0};
        sprintf(path, "/dev/sd%d", slot);

        // mount volume
        int status = fat32_mount(path);
        if (status < 0)
        {
            printk("[init] fat32_mount() returned %d!\r\n", status);
            continue;
        }
    }

    // dump fs
    vfs_dump_fs();

    // test fat32 read
    char buffer1[513];
    vfs_read("/volumes/NO NAME/INIT.SH", (uint8_t *)buffer1, 512, 0);
    buffer1[512] = '\0';
    printk("%s\r\n", buffer1);

    char buffer2[513];
    vfs_read("/volumes/NO NAME/fseventsd-uuid", (uint8_t *)buffer2, 512, 0);
    buffer2[512] = '\0';
    printk("%s\r\n", buffer2);

    size_t file_size = vfs_get_file_size("/volumes/NO NAME/DTB/OUT.DTS");
    char *buffer3 = (char *)kmalloc(file_size + 1);
    vfs_read("/volumes/NO NAME/DTB/OUT.DTS", (uint8_t *)buffer3, file_size, 0);
    buffer3[file_size] = '\0';
    printk("%s\r\n", buffer3);

    kfree(buffer3);

    // test fat32 write
    vfs_write("/volumes/NO NAME/INIT.SH", NULL, 0, 0);
    vfs_write("/volumes/NO NAME/fseventsd-uuid", NULL, 0, 0);

    syscall_exit(0);
}
