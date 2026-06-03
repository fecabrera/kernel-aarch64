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
#include <dsa/queue.h>
#include "kernel.h"

// static char size_strs[4][10] = {"B", "KiB", "MiB", "GiB"};

void kernel_init()
{
    // Initialize DTB
    dtb_init();
    dtb_dump();

    // Initialize memory
    mem_init();
    heap_init();
    heap_dump();

    // Initialize interrupts
    gic_init();
    virtio_mmio_init();
    irq_init();
    pl011_init();
    pl031_init();
    scheduler_init();
    irq_enable();

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

    time_t t0 = syscall_uptime();
    initialize_ramdisk();
    time_t t1 = syscall_uptime();

    printk("[init] took %dms\r\n", t1 - t0);

    syscall_exit(0);
}

void initialize_ramdisk()
{
    int slot = virtio_mmio_find_next_slot(VIRTIO_DEVICE_ID_BLOCK, -1);
    if (slot < 0)
    {
        printk("[init] no block device found!\r\n");
        return;
    }

    printk("[init] slot %i\r\n", slot);

    uint8_t *buff = (uint8_t *)kmalloc(512);
    uint32_t status;

    // read boot sector
    status = virtio_mmio_read(slot, 0, buff);
    if (status != VIRTIO_BLK_S_OK)
    {
        printk("[init] virtio_mmio_read() returned %i!\r\n", status);
        kfree(buff);
        return;
    }

    // check boot sector signature
    if (!fat32_is_boot_sector(buff))
    {
        printk("[init] not a FAT32 boot sector!\r\n");
        kfree(buff);
        return;
    }

    // parse boot sector
    struct fat32_bs_info *bs_info = (struct fat32_bs_info *)kmalloc(sizeof(struct fat32_bs_info));
    fat32_parse_boot_sector(buff, bs_info);

    printk("volume \"%s\":\r\n", bs_info->volume_label);
    printk("  size              = %d B\r\n", bs_info->total_sectors_32 * bs_info->n_bytes_per_sector);
    printk("  drive_number      = 0x%02x\r\n", bs_info->drive_number);
    printk("  table_size        = %d\r\n", bs_info->table_size_32);
    printk("  first_fat_sector  = %d\r\n", bs_info->first_fat_sector);
    printk("  first_data_sector = %d\r\n", bs_info->first_data_sector);
    printk("  root_cluster      = %d\r\n", bs_info->root_cluster);
    printk("  table_size_32     = %d\r\n", bs_info->table_size_32);
    printk("  data_sectors      = %d\r\n", bs_info->data_sectors);
    printk("  total_clusters    = %d\r\n", bs_info->total_clusters);
    printk("\r\n");

    // read fat table
    fat_table_entry_t *fat_table = (fat_table_entry_t *)kmalloc(bs_info->table_size_32 * sizeof(fat_table_entry_t));
    if (fat_table == NULL)
    {
        printk("[init] kmalloc() failed for fat_table!\r\n");
        kfree(buff);
        kfree(bs_info);
        return;
    }

    // parse fat table sector by sector, copying entries into fat_table.
    // We need to read the FAT32 table one sector at a time because the virtio_mmio_read()
    // buffer must be 512-byte aligned, and the FAT32 table entries are 4 bytes each so a
    // sector contains 128 entries.
    uint16_t n_clusters_per_sector = bs_info->n_bytes_per_sector / 4;

    uint32_t i = 0;
    while (i < bs_info->table_size_32)
    {
        uint32_t sector_offset = i / n_clusters_per_sector;

        status = virtio_mmio_read(slot, bs_info->first_fat_sector + sector_offset, (uint8_t *)buff);
        if (status != VIRTIO_BLK_S_OK)
        {
            printk("[init] virtio_mmio_read() returned %i!\r\n", status);
            kfree(buff);
            kfree(fat_table);
            kfree(bs_info);
            return;
        }

        i += fat32_read_fat_table(bs_info, buff, sector_offset, fat_table);
    }

    printk("copied %d entries to fat table\r\n", i);

    struct queue64 fat_q;
    queue64_init(&fat_q, 10);

    for (uint32_t i = 0; i < bs_info->table_size_32; i++)
    {
        uint32_t value = fat_table[i] & 0x0FFFFFFF;
        if (value == FAT32_FAT_ENTRY_RESERVED || value == FAT32_FAT_ENTRY_BAD)
            break;

        queue64_push(&fat_q, i);

        while (value < FAT32_FAT_ENTRY_EOC && i < bs_info->table_size_32)
            i++;
    }

    printk("clusters = [");
    for (uint32_t i = 0; i < fat_q.length; i++)
    {
        uint64_t cluster = queue64_at(&fat_q, i);
        if (i)
            printk(",");
        printk(" %d", cluster);
    }
    printk(" ]\r\n");

    // create root node
    struct fs_node *root = fs_create_folder(bs_info->volume_label, strnlen(bs_info->volume_label, 11), 0);

    // read directories
    for (uint32_t i = bs_info->root_cluster; i < fat_q.length; i++)
    {
        uint32_t cluster = queue64_at(&fat_q, i);

        uint32_t j = 0;
        while (1)
        {
            cluster += j;
            uint32_t sector_offset = cluster - bs_info->root_cluster;

            printk("cluster %d (0x%08x):\r\n", cluster, (bs_info->first_data_sector + sector_offset) * bs_info->n_bytes_per_sector);

            status = virtio_mmio_read(slot, bs_info->first_data_sector + sector_offset, buff);
            if (status != VIRTIO_BLK_S_OK)
            {
                printk("[init] virtio_mmio_read() returned %i!\r\n", status);
                kfree(buff);
                kfree(fat_table);
                kfree(bs_info);
                return;
            }

            if (fat32_read_cluster(bs_info, buff, root) != 0)
                break;

            j++;
        }
    }

    fs_dump_node(root);

    queue64_destroy(&fat_q);
    kfree(fat_table);
    kfree(buff);
    kfree(bs_info);
}

void child()
{
    pid_t child_pid = syscall_fork();

    if (child_pid < 0)
    {
        printk("[child] fork() failed!\r\n");
        syscall_exit(2);
    }

    pid_t pid = syscall_getpid();
    printk("[child] pid = %i, child_pid = %i\r\n", pid, child_pid);

    if (child_pid == 0)
    {
        printk("[child] going to sleep\r\n");

        time_t t0 = syscall_uptime();
        syscall_sleep(2);
        time_t t1 = syscall_uptime();

        printk("[child] slept for %dms\r\n", t1 - t0);
        syscall_exit(0);
    }
    else
    {
        syscall_exit(1);
    }
}
