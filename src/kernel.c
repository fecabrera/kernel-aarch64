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

    initialize_ramdisk();

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

    // parse boot sector
    struct fat32_bs_info bs_info = {0};
    fat32_parse_boot_sector(buff, &bs_info);

    printk("volume \"%s\":\r\n", bs_info.volume_label);
    printk("  size              = %d B\r\n", bs_info.total_sectors_32 * bs_info.n_bytes_per_sector);
    printk("  drive_number      = 0x%02x\r\n", bs_info.drive_number);
    printk("  table_size        = %d\r\n", bs_info.table_size_32);
    printk("  first_fat_sector  = %d\r\n", bs_info.first_fat_sector);
    printk("  first_data_sector = %d\r\n", bs_info.first_data_sector);
    printk("  root_cluster      = %d\r\n", bs_info.root_cluster);
    printk("  data_sectors      = %d\r\n", bs_info.data_sectors);
    printk("  total_clusters    = %d\r\n", bs_info.total_clusters);
    printk("\r\n");

    // read fat sectors
    uint32_t *fat_table = (uint32_t *)kmalloc(bs_info.table_size_32 * sizeof(uint32_t));
    if (fat_table == NULL)
    {
        printk("[init] kmalloc() failed for fat_table!\r\n");
        kfree(buff);
        return;
    }

    uint32_t n_sectors_to_read = 4 * bs_info.table_size_32 / bs_info.n_bytes_per_sector;
    uint16_t n_entries_per_sector = bs_info.n_bytes_per_sector / 4;

    printk("n_sectors_to_read = %d\r\n", n_sectors_to_read);

    for (uint32_t i = 0; i < bs_info.table_size_32; i++)
    {
        uint32_t sector = (i * 4) / bs_info.n_bytes_per_sector;
        status = virtio_mmio_read(slot, bs_info.first_fat_sector + sector, (uint8_t *)buff);

        printk("clusters %d-%d:\r\n", sector * bs_info.n_bytes_per_sector / 4, ((sector + 1) * bs_info.n_bytes_per_sector / 4) - 1);

        uint32_t *cluster = (uint32_t *)buff;
        for (int j = 0; j < n_entries_per_sector && i < bs_info.table_size_32; j++)
        {
            int offset = i++ % bs_info.n_bytes_per_sector;
            fat_table[i] = cluster[j] & 0x0FFFFFFF;
            if (fat_table[i] > 0)
                printk("  #%d: 0x%08x\r\n", offset, fat_table[i]);
        }
        printk("\r\n");
    }

    // read root dir sector
    printk("first_data_sector = %d\r\n", bs_info.first_data_sector);
    status = virtio_mmio_read(slot, bs_info.first_data_sector, buff);

    if (status != VIRTIO_BLK_S_OK)
    {
        printk("[init] virtio_mmio_read() returned %i!\r\n", status);
        kfree(buff);
        kfree(fat_table);
        return;
    }

    printk("root dir:\r\n");

    int idx = 0, cnt = 0;
    while (idx < 16)
    {
        struct fat32_lfn_entry *root_dir_lfn = NULL;
        struct fat32_dir_entry *root_dir = ((struct fat32_dir_entry *)buff) + idx++;

        if (root_dir->name[0] == FAT32_DIRENT_FREE)
            continue;

        if (root_dir->name[0] == FAT32_DIRENT_END)
            break;

        cnt++;

        uint8_t attributes;
        memcpy(&attributes, &root_dir->attributes, 1);

        // dump root dir entry
        if (attributes == FAT32_ATTR_LFN)
        {
            idx++;
            root_dir_lfn = (struct fat32_lfn_entry *)root_dir;
            root_dir = (struct fat32_dir_entry *)root_dir + 1;
            memcpy(&attributes, &root_dir->attributes, 1);
            fat32_dump_lfn_entry(root_dir_lfn);
        }
        else
        {
            fat32_dump_dir_entry(root_dir);
        }

        uint16_t cluster_low, cluster_high;
        uint32_t file_size;
        memcpy(&cluster_low, &root_dir->cluster_low, 2);
        memcpy(&cluster_high, &root_dir->cluster_high, 2);
        memcpy(&file_size, &root_dir->file_size, 4);

        char dir_name[12] = {0};
        char lfn_dir_name[52] = {0};
        char16_t _lfn_dir_name[26] = {0};

        strncpy(dir_name, (char *)root_dir->name, 11);
        if (root_dir_lfn != NULL)
        {
            memcpy(_lfn_dir_name, root_dir_lfn->name1, 10);
            memcpy(_lfn_dir_name + 5, root_dir_lfn->name2, 12);
            memcpy(_lfn_dir_name + 11, root_dir_lfn->name3, 4);
            utf16lencpy(lfn_dir_name, (char16_t *)_lfn_dir_name, 26);
        }

        printk("%d:\r\n", cnt);
        printk("  name          = \"%s\"\r\n", dir_name);
        if (root_dir_lfn != NULL)
            printk("  lfn_name      = \"%s\"\r\n", lfn_dir_name);
        printk("  attributes    = 0x%02x\r\n", attributes);
        printk("  cluster       = %d\r\n", ((uint32_t)_le16(cluster_high) << 16) | _le16(cluster_low));
        printk("  size          = %d B\r\n", _le32(file_size));
        printk("\r\n");
    }

    kfree(buff);
    kfree(fat_table);
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
