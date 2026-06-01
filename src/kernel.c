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

static char size_strs[4][10] = {"B", "KiB", "MiB", "GiB"};

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

    uint8_t buff[512];
    uint32_t status;

    status = virtio_mmio_read(slot, 0, (uint8_t *)&buff);

    if (status != VIRTIO_BLK_S_OK)
    {
        printk("[init] virtio_mmio_read() returned %i!\r\n", status);
        return;
    }

    struct mbr_boot_sector *bs = (struct mbr_boot_sector *)buff;
    struct fat32_extended_boot_record *ext_br = (struct fat32_extended_boot_record *)&bs->boot_code;

    // dump boot sector
    fat32_dump_boot_sector(bs);

    // volume name
    char volume_label[12] = {0};
    strncpy(volume_label, (char *)ext_br->volume_label, 11);

    uint8_t drive_number;
    memcpy(&drive_number, &ext_br->drive_number, 1);

    uint16_t n_bytes_per_sector;
    uint32_t total_sectors_32;
    memcpy(&n_bytes_per_sector, &bs->n_bytes_per_sector, 2);
    memcpy(&total_sectors_32, &bs->total_sectors_32, 4);

    uint32_t size = total_sectors_32 * n_bytes_per_sector;

    // find root dir
    uint32_t root_cluster, table_size_32;
    uint16_t n_reserved_sectors;
    uint8_t n_sectors_per_cluster, n_fat;
    memcpy(&root_cluster, &ext_br->root_cluster, 4);
    memcpy(&n_sectors_per_cluster, &bs->n_sectors_per_cluster, 1);
    memcpy(&n_reserved_sectors, &bs->n_reserved_sectors, 2);
    memcpy(&table_size_32, &ext_br->table_size_32, 4);
    memcpy(&n_fat, &bs->n_fat, 1);

    // extract data
    uint32_t first_fat_sector = n_reserved_sectors;
    uint32_t first_data_sector = n_reserved_sectors + (table_size_32 * n_fat);

    uint32_t data_sectors = total_sectors_32 - first_data_sector;
    uint32_t total_clusters = data_sectors / n_sectors_per_cluster;

    printk("volume \"%s\":\r\n", volume_label);
    printk("  size              = %d B\r\n", size);
    printk("  drive_number      = 0x%02x\r\n", drive_number);
    printk("  n_fat             = %d\r\n", n_fat);
    printk("  table_size        = %d\r\n", table_size_32);
    printk("  first_fat_sector  = %d\r\n", n_reserved_sectors);
    printk("  first_data_sector = %d\r\n", first_data_sector);
    printk("  root_cluster      = %d\r\n", root_cluster);
    printk("  data_sectors      = %d\r\n", data_sectors);
    printk("  total_clusters    = %d\r\n", total_clusters);
    printk("\r\n");

    // read fat sectors
    int n_sectors_to_read = 4 * table_size_32 / n_bytes_per_sector;
    for (int i = 0; i < n_sectors_to_read; i++)
    {
        printk("clusters %d-%d:\r\n", i * n_bytes_per_sector / 4, ((i + 1) * n_bytes_per_sector / 4) - 1);
        status = virtio_mmio_read(slot, first_fat_sector + i, (uint8_t *)&buff);

        uint32_t *cluster = (uint32_t *)buff;

        int n_entries_per_sector = n_bytes_per_sector / 4;
        for (int j = 0; j < n_entries_per_sector; j++)
        {
            uint32_t value = cluster[j] & 0x0FFFFFFF;
            if (value > 0)
                printk("  #%d: 0x%08x\r\n", j, value);
        }
        printk("\r\n");
    }

    // read root dir sector
    status = virtio_mmio_read(slot, first_data_sector, (uint8_t *)&buff);

    if (status != VIRTIO_BLK_S_OK)
    {
        printk("[init] virtio_mmio_read() returned %i!\r\n", status);
        return;
    }

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
