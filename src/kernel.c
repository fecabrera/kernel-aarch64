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
#include "kernel.h"

static void child()
{
    pid_t child_pid = syscall_fork();

    if (child_pid < 0)
    {
        printk("[child] fork() failed!\r\n");
        syscall_exit(2);
    }

    pid_t pid = syscall_getpid();
    printk("[child] pid = %i, child_pid = %i\r\n", pid, child_pid);

    if (child_pid == 0) // children
    {
        printk("[child] going to sleep\r\n");

        time_t t0 = syscall_uptime();
        syscall_sleep(2);
        time_t t1 = syscall_uptime();

        printk("[child] slept for %dms\r\n", t1 - t0);
        syscall_exit(0);
    }
    else // parent
    {
        syscall_exit(1);
    }
}

static void test_scheduler()
{
    pid_t fork_pid = syscall_fork();

    if (fork_pid < 0)
    {
        printk("[child] fork() failed!\r\n");
        halt();
    }

    if (fork_pid == 0)
        return child();

    int64_t exit_status = syscall_waitpid(fork_pid);
    printk("[init] child process (pid %i) terminated with status %i\r\n", fork_pid, exit_status);
}

static int test_vfs_read(struct fs_node *node, uint8_t *buff, size_t n)
{
    printk("test_vfs_read(): node=0x%08x, buff=0x%08x, n=%d\r\n", node->name, buff, n);

    char *msg = "im pibble";
    strncpy((char *)buff, msg, strlen(msg));

    printk("test_vfs_read(): sent \"%s\"\r\n", msg);

    return 0;
}

static int test_vfs_write(struct fs_node *node, uint8_t *buff, size_t n)
{
    printk("test_vfs_write(): node=0x%08x, buff=0x%08x, n=%d\r\n", node->name, buff, n);

    char msg[50] = {0};
    strncpy(msg, (char *)buff, n);

    printk("test_vfs_write(): received \"%s\"\r\n", msg);

    return 0;
}

static void test_vfs()
{
    printk("\r\n=== vfs test ===\r\n");

    printk("mounting \"/test\"...\r\n");
    vfs_create_dir("/", "test", 0);
    vfs_create_mountpoint("/test", &test_vfs_read, &test_vfs_write, NULL);

    printk("creating \"/test/file\"...\r\n");
    vfs_create_file("/test", "file", 0);

    printk("reading \"/test/file\"...\r\n");
    char buffer[50] = {0};
    vfs_read("/test/file", (uint8_t *)&buffer, 50);

    printk("writing \"/test/file\"...\r\n");
    char *msg = "wash my belly";
    vfs_write("/test/file", (uint8_t *)msg, strlen(msg));

    printk("dumping vfs tree...\r\n");
    vfs_dump_fs();

    printk("unmounting \"/test\"...\r\n");
    vfs_destroy_mountpoint("/test");

    if (vfs_get_mountpoint_for_path("/test"))
        printk("mountpoint \"/test\" still exists :/\r\n");
    else
        printk("mountpoint \"/test\" was removed :)\r\n");

    printk("================\r\n\r\n");
}

static int test_io_read(uint8_t *buff, size_t n, uint64_t drv_info)
{
    printk("test_io_read(): buff=0x%08x, n=%d, drv_info=0x%08x\r\n", buff, n, drv_info);

    char *msg = "im pibble";
    strncpy((char *)buff, msg, strlen(msg));

    printk("test_io_read(): received \"%s\"\r\n", msg);

    return 0;
}

static int test_io_write(uint8_t *buff, size_t n, uint64_t drv_info)
{
    printk("test_io_write(): buff=0x%08x, n=%d, drv_info=0x%08x\r\n", buff, n, drv_info);

    char msg[50] = {0};
    strncpy(msg, (char *)buff, n);

    printk("test_io_write(): received \"%s\"\r\n", msg);

    return 0;
}

static void test_io_modules()
{
    printk("\r\n=== io test ===\r\n");

    io_register_module("test", 0, &test_io_read, &test_io_write);

    printk("dumping vfs tree...\r\n");
    vfs_dump_fs();

    printk("reading \"/dev/test\"...\r\n");
    char buffer[50] = {0};
    vfs_read("/dev/test", (uint8_t *)&buffer, 50);

    printk("writing \"/dev/test\"...\r\n");
    char *msg = "wash my belly";
    vfs_write("/dev/test", (uint8_t *)msg, strlen(msg));

    io_unregister_module("test");

    printk("===============\r\n\r\n");
}

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

    test_scheduler();
    test_vfs();
    test_io_modules();

    time_t t0 = syscall_uptime();
    virtio_slot_t slot = -1;
    while ((slot = virtio_mmio_find_next_slot(VIRTIO_DEVICE_ID_BLOCK, slot)) != -1)
    {
        printk("[init] reading block device in slot %i\r\n", slot);
        virtio_mmio_initialize_fat32_device(slot);
    }
    time_t t1 = syscall_uptime();

    printk("[init] took %dms\r\n", t1 - t0);

    syscall_exit(0);
}
