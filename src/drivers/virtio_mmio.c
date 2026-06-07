#include <debug.h>
#include <dtb.h>
#include <string.h>
#include <stdlib.h>
#include <arch/cpu.h>
#include <arch/syscall.h>
#include <mm/heap.h>
#include <fs/fat32.h>
#include <fs/filesystem.h>
#include <fs/vfs.h>
#include <dsa/queue.h>
#include <dsa/set.h>
#include <dsa/stack.h>
#include "gic.h"
#include "virtio_mmio.h"

static virtio_slot_t _irq_to_slot[256] = {0};
static virtio_mmio_handler_t _handlers[32];
static struct virtio_mmio_device _devices[32] = {0};

static int _virtio_mmio_probe_device(int slot)
{
    struct virtio_mmio_device *device = &_devices[slot];

    uint32_t magic = VIRTIO_MAGIC(slot);
    uint32_t version = VIRTIO_VERSION(slot);
    uint32_t device_id = VIRTIO_DEVICE_ID(slot);

    // parse magic
    if (magic != VIRTIO_MMIO_MAGIC)
        return VIRTIO_MMIO_BAD_MAGIC;

    // parse version
    if (version != VIRTIO_MMIO_VERSION_MODERN)
        return VIRTIO_MMIO_UNSUPPORTED_VERSION;

    // parse device ID
    if (device_id == VIRTIO_DEVICE_ID_INVALID)
        return VIRTIO_MMIO_INVALID_DEVICE;

    dprintk("[virtio_mmio@%x] magic = 0x%x, version = %i, device_id = %i\r\n", VIRTIO_MMIO_ADDR(slot), magic, version, VIRTIO_DEVICE_ID(slot));

    // acknowledge
    VIRTIO_STATUS(slot) = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

    // get features
    uint32_t features_lo, features_hi;

    VIRTIO_DEVICE_FEATURES_SEL(slot) = VIRTIO_FEATURES_LOW;
    features_lo = VIRTIO_DEVICE_FEATURES(slot);

    VIRTIO_DEVICE_FEATURES_SEL(slot) = VIRTIO_FEATURES_HIGH;
    features_hi = VIRTIO_DEVICE_FEATURES(slot);

    dprintk("[virtio_mmio@%x] features: low=0x%x, high=0x%x\r\n", VIRTIO_MMIO_ADDR(slot), features_lo, features_hi);

    // negociate features
    dprintk("[virtio_mmio@%x] negociating features\r\n", VIRTIO_MMIO_ADDR(slot));

    VIRTIO_DRIVER_FEATURES_SEL(slot) = VIRTIO_FEATURES_LOW;
    VIRTIO_DRIVER_FEATURES(slot) = features_lo & (VIRTIO_BLK_F_BLK_SIZE | VIRTIO_BLK_F_SIZE_MAX);

    VIRTIO_DRIVER_FEATURES_SEL(slot) = VIRTIO_FEATURES_HIGH;
    VIRTIO_DRIVER_FEATURES(slot) = features_hi & VIRTIO_F_VERSION_1; // must keep this

    VIRTIO_STATUS(slot) |= VIRTIO_STATUS_FEATURES_OK;

    // check response
    if (!(VIRTIO_STATUS(slot) & VIRTIO_STATUS_FEATURES_OK))
    {
        dprintk("[virtio_mmio@%x] features rejected\r\n", VIRTIO_MMIO_ADDR(slot));
        VIRTIO_STATUS(slot) |= VIRTIO_STATUS_FAILED;

        return VIRTIO_MMIO_NEGOTIATION_FAILED; // negotiation failed
    }

    // negotiation accepted
    dprintk("[virtio_mmio@%x] features accepted\r\n", VIRTIO_MMIO_ADDR(slot));

    // set up virtqueue
    VIRTIO_QUEUE_SEL(slot) = 0;

    uint32_t virtio_queue_num_max = VIRTIO_QUEUE_NUM_MAX(slot);
    dprintk("[virtio_mmio@%x] virtio_queue_num_max=%i\r\n", VIRTIO_MMIO_ADDR(slot), virtio_queue_num_max);

    if (virtio_queue_num_max < DEFAULT_VIRTIO_QUEUE_NUM)
    {
        dprintk("[virtio_mmio@%x] device doesn't support queue_num=%i\r\n", VIRTIO_MMIO_ADDR(slot), DEFAULT_VIRTIO_QUEUE_NUM);
        return VIRTIO_MMIO_QUEUE_NUM_ERROR;
    }

    VIRTIO_QUEUE_NUM(slot) = DEFAULT_VIRTIO_QUEUE_NUM;

    struct virtq *q = &device->queue;

    // Write physical addresses (split into low/high 32-bit halves)
    VIRTIO_QUEUE_DESC_LOW(slot) = (uint32_t)((uintptr_t)&q->desc);
    VIRTIO_QUEUE_DESC_HIGH(slot) = (uint32_t)((uintptr_t)&q->desc >> 32);

    VIRTIO_QUEUE_DRIVER_LOW(slot) = (uint32_t)((uintptr_t)&q->avail);
    VIRTIO_QUEUE_DRIVER_HIGH(slot) = (uint32_t)((uintptr_t)&q->avail >> 32);

    VIRTIO_QUEUE_DEVICE_LOW(slot) = (uint32_t)((uintptr_t)&q->used);
    VIRTIO_QUEUE_DEVICE_HIGH(slot) = (uint32_t)((uintptr_t)&q->used >> 32);

    // Mark the queue live
    VIRTIO_QUEUE_READY(slot) = 1;

    // initialize IRQ
    uint32_t virtio_mmio_irq;
    if (dtb_get_virtio_mmio_irq_number(slot, &virtio_mmio_irq) != 0)
    {
        dprintk("[virtio_mmio] IRQ not found!!\r\n");
        return VIRTIO_MMIO_IRQ_NOT_FOUND;
    }

    _irq_to_slot[virtio_mmio_irq] = slot;

    dprintk("[virtio_mmio@%x] Initializing IRQ: %i\r\n", VIRTIO_MMIO_ADDR(slot), virtio_mmio_irq);
    irq_register_handler(virtio_mmio_irq, &virtio_mmio_irq_handler);
    gic_enable_irq(virtio_mmio_irq);

    // if everything goes well we add it to our table
    device->irq_id = virtio_mmio_irq;
    device->device_id = device_id;

    return 0;
}

void virtio_mmio_init()
{
    for (virtio_slot_t slot = 0; slot < 32; slot++)
        _virtio_mmio_probe_device(slot);
}

virtio_slot_t virtio_mmio_find_next_slot(uint32_t device_id, int start)
{
    for (int idx = start + 1; idx < 32; idx++)
        if (_devices[idx].device_id == device_id)
            return idx;

    return -1;
}

int virtio_mmio_read(virtio_slot_t slot, uint64_t sector_number, uint8_t *data)
{
    struct virtio_mmio_device *device = &_devices[slot];

    if (device->device_id == VIRTIO_DEVICE_ID_INVALID)
    {
        dprintk("[virtio_mmio] Device slot %i is invalid!\r\n", slot);
        return VIRTIO_MMIO_INVALID_DEVICE;
    }

    // 1. Fill the header
    struct virtio_blk_req_header hdr = {
        .type = 0,
        .sector = sector_number,
    };
    uint8_t status = 0xFF; // device writes 0 on success, 1 on error, 2 = unsupported

    struct virtq *q = &device->queue;

    // 2. Fill descriptor chain (indices 0, 1, 2)
    q->desc[0].addr = (uintptr_t)&hdr;
    q->desc[0].len = sizeof(hdr);
    q->desc[0].flags = VIRTQ_DESC_F_NEXT;
    q->desc[0].next = 1;

    q->desc[1].addr = (uintptr_t)data;
    q->desc[1].len = VIRTIO_MMIO_BLK_SECTOR_SIZE;
    q->desc[1].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
    q->desc[1].next = 2;

    q->desc[2].addr = (uintptr_t)&status;
    q->desc[2].len = 1;
    q->desc[2].flags = VIRTQ_DESC_F_WRITE;

    // 3. Post to available ring
    uint16_t idx = q->avail.idx % DEFAULT_VIRTIO_QUEUE_NUM;
    q->avail.ring[idx] = 0; // head descriptor index
    q->avail.idx++;

    // 4. Kick the device
    VIRTIO_QUEUE_NOTIFY(slot) = 0;

    // 5. Poll or wait for IRQ — device writes to used ring when done
    // In the IRQ handler, check used.idx advanced and read status byte
    while (status == VIRTIO_BLK_S_NONE)
        syscall_msleep(1);

    dprintk("[virtio_mmio@%x] status = %u\r\n", VIRTIO_MMIO_ADDR(slot), status);

    return status;
}

struct cpu_context *virtio_mmio_irq_handler(int irq, struct cpu_context *ctx)
{
    virtio_slot_t slot = _irq_to_slot[irq];
    virtio_mmio_handler_t fnc = _handlers[slot];

    uint32_t status = VIRTIO_INTERRUPT_STATUS(slot);
    VIRTIO_INTERRUPT_ACK(slot) = status;

    dprintk("[virtio_mmio@%x] IRQ handler, status = %u\r\n", VIRTIO_MMIO_ADDR(slot), status);

    if (fnc == NULL)
    {
        dprintk("[virtio_mmio@%x] Handler not found for slot %i!\r\n", VIRTIO_MMIO_ADDR(slot), slot);
        return ctx;
    }

    return fnc(slot, ctx);
}

static int64_t _virtio_mmio_fat32_parse_table(virtio_slot_t slot, struct fat32_bs_info *bs_info, fat_table_entry_t *fat_table)
{
    // allocate buffer
    uint8_t *buff = (uint8_t *)kmalloc(bs_info->n_bytes_per_sector);

    // parse fat table sector by sector, copying entries into fat_table.
    uint16_t n_clusters_per_sector = bs_info->n_bytes_per_sector / 4;

    int64_t i = 0;
    while (i < bs_info->table_size_32)
    {
        // We need to read the FAT32 table one sector at a time since the virtio_mmio_read()
        // buffer must be 512-byte aligned, and the FAT32 table entries are 4 bytes each so a
        // sector contains 128 entries.
        uint32_t sector_offset = i / n_clusters_per_sector;

        uint32_t status = virtio_mmio_read(slot, bs_info->first_fat_sector + sector_offset, (uint8_t *)buff);
        if (status != VIRTIO_BLK_S_OK)
        {
            dprintk("[virtio_mmio@%x] virtio_mmio_read() returned %i!\r\n", VIRTIO_MMIO_ADDR(slot), status);
            kfree(buff);
            return -1;
        }

        i += fat32_read_fat_table(bs_info, buff, sector_offset, fat_table);
    }

    dprintk("copied %d entries to fat table\r\n", i);
    kfree(buff);
    return i;
}

int _virtio_mmio_fat32_read_cluster(virtio_slot_t slot, struct fat32_bs_info *bs_info, uint32_t cluster, struct fs_node *parent_node, struct set64 *parent_nodes)
{
    uint32_t sector_offset = cluster - bs_info->root_cluster;
    uint32_t sector = bs_info->first_data_sector + sector_offset;

    dprintk("cluster %d (0x%08x):\r\n", cluster, sector * bs_info->n_bytes_per_sector);
    uint16_t n_entries_per_sector = bs_info->n_bytes_per_sector / 32;

    // allocate buffer
    uint8_t *buff = (uint8_t *)kmalloc(bs_info->n_bytes_per_sector);

    int status = virtio_mmio_read(slot, sector, buff);
    if (status != VIRTIO_BLK_S_OK)
    {
        dprintk("[virtio_mmio@%x] virtio_mmio_read() returned %i!\r\n", VIRTIO_MMIO_ADDR(slot), status);
        kfree(buff);
        return -1;
    }

    struct fat32_dir_entry *first_entry = (struct fat32_dir_entry *)buff;
    for (uint32_t offset = 0; offset < n_entries_per_sector; offset++)
    {
        struct fat32_dir_entry *dir_entry = first_entry + offset;

        if ((dir_entry->name[0] == FAT32_DIRENT_END))
        {
            kfree(buff);
            return 1;
        }

        if ((dir_entry->name[0] == FAT32_DIRENT_FREE) ||
            (dir_entry->name[0] == FAT32_ATTR_SYSTEM) ||
            (dir_entry->name[0] == FAT32_ATTR_VOLUME_ID))
            continue;

        dprintk("entry #%d: \r\n", offset);

        struct stack64 lfn_entries;
        stack64_init(&lfn_entries, 10);

        // copy offset
        uint32_t _offset = offset;

        // parse lfn entries
        uint8_t attributes;
        int n_lfn_entries = 0;
        while (1)
        {
            memcpy(&attributes, &dir_entry->attributes, 1);

            if (attributes != FAT32_ATTR_LFN)
                break;

            offset++;
            n_lfn_entries++;

            struct fat32_lfn_entry *lfn_entry = (struct fat32_lfn_entry *)dir_entry;
            stack64_push(&lfn_entries, (uintptr_t)lfn_entry);

            dir_entry = (struct fat32_dir_entry *)dir_entry + 1;
        }

        memcpy(&attributes, &dir_entry->attributes, 1);

        char dir_name[12] = {0};
        char *lfn_dir_name = (char *)kmalloc(n_lfn_entries * 13 + 1);

        strncpy(dir_name, (char *)dir_entry->name, 12);

        // parse lfn name
        for (int i = 0; i < n_lfn_entries; i++)
        {
            struct fat32_lfn_entry *lfn_entry = (struct fat32_lfn_entry *)stack64_pop(&lfn_entries);

            char16_t _lfn_dir_name[26] = {0};
            memcpy(_lfn_dir_name, lfn_entry->name1, 10);
            memcpy(_lfn_dir_name + 5, lfn_entry->name2, 12);
            memcpy(_lfn_dir_name + 11, lfn_entry->name3, 4);

            utf16lencpy(lfn_dir_name + (i * 13), (char16_t *)_lfn_dir_name, 26);
        }

        uint16_t cluster_low, cluster_high;
        uint32_t file_size;
        memcpy(&cluster_low, &dir_entry->cluster_low, 2);
        memcpy(&cluster_high, &dir_entry->cluster_high, 2);
        memcpy(&file_size, &dir_entry->file_size, 4);

        uint32_t next_cluster = ((uint32_t)_le16(cluster_high) << 16) | _le16(cluster_low);

        // get actual name
        char *name;
        if (n_lfn_entries)
        {
            size_t len = strlen(lfn_dir_name);
            name = (char *)kmalloc(len + 1);
            strncpy(name, lfn_dir_name, len + 1);
        }
        else
        {
            name = (char *)kmalloc(13);
            size_t name_len = strntrimend(name, dir_name, 8);
            if (strntrimend(name + name_len + 1, dir_name + 8, 3) > 0)
                name[name_len] = '.';
            else
                name[name_len] = '\0';
        }

        if (strncmp(name, ".", 2) && strncmp(name, "..", 3))
        {
            struct fat32_entry_reference *entry_ref = (struct fat32_entry_reference *)kmalloc(sizeof(struct fat32_entry_reference));
            entry_ref->cluster = cluster;
            entry_ref->offset = _offset;

            struct fs_node *node;
            if (attributes & FAT32_ATTR_DIRECTORY)
                node = fs_add_subfolder(parent_node, name, 0, (uintptr_t)entry_ref);
            else
                node = fs_add_file_to_folder(parent_node, name, 0, (uintptr_t)entry_ref);

            set64_set(parent_nodes, next_cluster, (uintptr_t)node);
        }

        dprintk("  name          = \"%s\"\r\n", name);
        dprintk("  dir_name      = \"%s\"\r\n", dir_name);
        if (n_lfn_entries)
            dprintk("  lfn_name      = \"%s\"\r\n", lfn_dir_name);
        dprintk("  attributes    = 0x%02x\r\n", attributes);
        dprintk("  next_cluster  = %d\r\n", next_cluster);
        dprintk("  size          = %d B\r\n", _le32(file_size));
        dprintk("\r\n");

        kfree(lfn_dir_name);
        kfree(name);
    }

    kfree(buff);
    return 0;
}

static int _virtio_mmio_fat32_build_fs_tree(virtio_slot_t slot, struct fat32_bs_info *bs_info, struct queue64 *fat_q, struct fs_node *root_node)
{
    // create set of parent nodes
    struct set64 parent_nodes;
    set64_init(&parent_nodes, 10);

    // read directories
    for (uint32_t i = bs_info->root_cluster; i < fat_q->length; i++)
    {
        // next cluster chain
        struct fat32_cluster_chain *chain = (struct fat32_cluster_chain *)queue64_at(fat_q, i);

        // retrieve parent node
        uint64_t parent_node_ptr;
        struct fs_node *parent_node;
        if (set64_get(&parent_nodes, chain->start, &parent_node_ptr) == 0)
            parent_node = root_node;
        else
            parent_node = (struct fs_node *)parent_node_ptr;

        // ignore file contents
        if ((parent_node->attrs & FS_NODE_ATTRS_TYPE_MASK) == FS_NODE_ATTRS_TYPE_FILE)
            continue;

        // go through all clusters on the cluster chain
        for (uint32_t cluster = chain->start; cluster <= chain->end; cluster++)
        {
            int status = _virtio_mmio_fat32_read_cluster(slot, bs_info, cluster, parent_node, &parent_nodes);
            if (status < 0)
            {
                set64_destroy(&parent_nodes);
                return -1;
            }
            if (status > 0)
                break;
        }
    }

    set64_destroy(&parent_nodes);
    return 0;
}

static int _virtio_mmio_fat32_parse_boot_sector(virtio_slot_t slot, struct fat32_bs_info *bs_info)
{
    uint8_t *buff = (uint8_t *)kmalloc(512);

    // read boot sector
    int status = virtio_mmio_read(slot, 0, buff);
    if (status != VIRTIO_BLK_S_OK)
    {
        dprintk("[virtio_mmio@%x] virtio_mmio_read() returned %i!\r\n", VIRTIO_MMIO_ADDR(slot), status);
        kfree(buff);
        return -1;
    }

    // check boot sector signature
    if (!fat32_is_boot_sector(buff))
    {
        dprintk("[virtio_mmio@%x] not a FAT32 boot sector!\r\n", VIRTIO_MMIO_ADDR(slot));
        kfree(buff);
        return -1;
    }

    // parse boot sector
    fat32_parse_boot_sector(buff, bs_info);

    dprintk("volume \"%s\":\r\n", bs_info->volume_label);
    dprintk("  size              = %d B\r\n", bs_info->total_sectors * bs_info->n_bytes_per_sector);
    dprintk("  drive_number      = 0x%02x\r\n", bs_info->drive_number);
    dprintk("  first_fat_sector  = %d\r\n", bs_info->first_fat_sector);
    dprintk("  first_data_sector = %d\r\n", bs_info->first_data_sector);
    dprintk("  root_cluster      = %d\r\n", bs_info->root_cluster);
    dprintk("  table_size_32     = %d\r\n", bs_info->table_size_32);
    dprintk("  data_sectors      = %d\r\n", bs_info->data_sectors);
    dprintk("  total_clusters    = %d\r\n", bs_info->total_clusters);
    dprintk("\r\n");

    kfree(buff);

    return 0;
}

struct fs_node *virtio_mmio_initialize_fat32_device(virtio_slot_t slot)
{
    int status;

    struct fat32_bs_info *bs_info = (struct fat32_bs_info *)kmalloc(sizeof(struct fat32_bs_info));
    if (bs_info == NULL)
    {
        dprintk("[virtio_mmio@%x] kmalloc() failed for bs_info!\r\n", VIRTIO_MMIO_ADDR(slot));
        return NULL;
    }

    if ((status = _virtio_mmio_fat32_parse_boot_sector(slot, bs_info)) < 0)
    {
        dprintk("[virtio_mmio@%x] _virtio_mmio_fat32_parse_boot_sector() returned %i!\r\n", VIRTIO_MMIO_ADDR(slot), status);
        kfree(bs_info);
        return NULL;
    }

    // read fat table
    dprintk("[virtio_mmio@%x] allocating %d bytes for the fat table\r\n", VIRTIO_MMIO_ADDR(slot), bs_info->table_size_32 * sizeof(fat_table_entry_t));

    fat_table_entry_t *fat_table = (fat_table_entry_t *)kmalloc(bs_info->table_size_32 * sizeof(fat_table_entry_t));
    if (fat_table == NULL)
    {
        dprintk("[virtio_mmio@%x] kmalloc() failed for fat_table!\r\n", VIRTIO_MMIO_ADDR(slot));
        kfree(bs_info);
        return NULL;
    }

    if (_virtio_mmio_fat32_parse_table(slot, bs_info, fat_table) < 0)
    {
        dprintk("[virtio_mmio@%x] _virtio_mmio_fat32_parse_table() failed for fat_table!\r\n", VIRTIO_MMIO_ADDR(slot));

        kfree(bs_info);
        kfree(fat_table);

        return NULL;
    }

    // build linked list
    struct queue64 fat_q;
    queue64_init(&fat_q, 10);

    fat32_build_cluster_chains(bs_info, fat_table, &fat_q);

    // create folder
    struct fs_node *root = vfs_create_dir("/volumes", bs_info->volume_label, 0);
    if (root == NULL)
    {
        printk("[virtio_mmio@%x] cannot create mountpoint \"%s\"!\r\n");
        return NULL;
    }

    // build volume name
    char mountpoint[50];
    sprintf(mountpoint, "/volumes/%s", bs_info->volume_label);

    // mount
    printk("[virtio_mmio@%x] mounting \"%s\"\r\n", VIRTIO_MMIO_ADDR(slot), mountpoint);
    vfs_create_mountpoint(mountpoint, NULL, NULL, NULL);

    status = _virtio_mmio_fat32_build_fs_tree(slot, bs_info, &fat_q, root);
    if (status < 0)
    {
        dprintk("[virtio_mmio@%x] _virtio_mmio_fat32_build_fs_tree() returned %i!\r\n", VIRTIO_MMIO_ADDR(slot), status);

        for (uint32_t i = bs_info->root_cluster; i < fat_q.length; i++)
            kfree((void *)queue64_at(&fat_q, i));

        queue64_destroy(&fat_q);
        fs_destroy_node(root);

        kfree(fat_table);
        kfree(bs_info);
        kfree(root);

        return NULL;
    }

    // dump fs
    vfs_dump_fs();

    // unmount
    printk("[virtio_mmio@%x] unmounting \"%s\"\r\n", VIRTIO_MMIO_ADDR(slot), mountpoint);
    vfs_destroy_mountpoint(mountpoint);

    // verify that mountpoint was removed
    if (vfs_get_mountpoint_for_path(mountpoint))
        printk("[virtio_mmio@%x] mountpoint \"%s\" still exists :/\r\n", VIRTIO_MMIO_ADDR(slot), mountpoint);
    else
        printk("[virtio_mmio@%x] mountpoint \"%s\" was removed :)\r\n", VIRTIO_MMIO_ADDR(slot), mountpoint);

    // dump fs
    vfs_dump_fs();

    // clean ptrs
    for (uint32_t i = bs_info->root_cluster; i < fat_q.length; i++)
        kfree((void *)queue64_at(&fat_q, i));

    queue64_destroy(&fat_q);

    kfree(fat_table);
    kfree(bs_info);

    return root;
}
