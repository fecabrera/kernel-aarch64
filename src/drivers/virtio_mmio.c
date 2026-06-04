#include <debug.h>
#include <dtb.h>
#include <string.h>
#include <arch/syscall.h>
#include <mm/heap.h>
#include <fs/fat32.h>
#include <fs/filesystem.h>
#include <dsa/queue.h>
#include <dsa/set.h>
#include "gic.h"
#include "virtio_mmio.h"

static int _irq_to_slot[256] = {0};
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
    for (int slot = 0; slot < 32; slot++)
        _virtio_mmio_probe_device(slot);
}

int virtio_mmio_find_next_slot(uint32_t device_id, int start)
{
    for (int idx = start + 1; idx < 32; idx++)
        if (_devices[idx].device_id == device_id)
            return idx;

    return -1;
}

int virtio_mmio_read(int slot, uint64_t sector_number, uint8_t *data)
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
    int slot = _irq_to_slot[irq];
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

struct fs_node *virtio_mmio_initialize_fat32_device(int slot)
{
    uint8_t *buff = (uint8_t *)kmalloc(512);
    uint32_t status;

    // read boot sector
    status = virtio_mmio_read(slot, 0, buff);
    if (status != VIRTIO_BLK_S_OK)
    {
        dprintk("[virtio_mmio@%x] virtio_mmio_read() returned %i!\r\n", VIRTIO_MMIO_ADDR(slot), status);
        kfree(buff);
        return NULL;
    }

    // check boot sector signature
    if (!fat32_is_boot_sector(buff))
    {
        dprintk("[virtio_mmio@%x] not a FAT32 boot sector!\r\n", VIRTIO_MMIO_ADDR(slot));
        kfree(buff);
        return NULL;
    }

    // parse boot sector
    struct fat32_bs_info *bs_info = (struct fat32_bs_info *)kmalloc(sizeof(struct fat32_bs_info));
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

    // read fat table
    printk("[virtio_mmio@%x] allocating %d bytes for the fat table\r\n", VIRTIO_MMIO_ADDR(slot), bs_info->table_size_32 * sizeof(fat_table_entry_t));
    fat_table_entry_t *fat_table = (fat_table_entry_t *)kmalloc(bs_info->table_size_32 * sizeof(fat_table_entry_t));
    if (fat_table == NULL)
    {
        dprintk("[virtio_mmio@%x] kmalloc() failed for fat_table!\r\n", VIRTIO_MMIO_ADDR(slot));
        kfree(buff);
        kfree(bs_info);
        return NULL;
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
            dprintk("[virtio_mmio@%x] virtio_mmio_read() returned %i!\r\n", VIRTIO_MMIO_ADDR(slot), status);
            kfree(buff);
            kfree(fat_table);
            kfree(bs_info);
            return NULL;
        }

        i += fat32_read_fat_table(bs_info, buff, sector_offset, fat_table);
    }

    dprintk("copied %d entries to fat table\r\n", i);

    // build linked list
    struct queue64 fat_q;
    queue64_init(&fat_q, 10);

    for (uint32_t i = 0; i < bs_info->table_size_32; i++)
    {
        uint32_t value = fat_table[i] & 0x0FFFFFFF;
        if (value == FAT32_FAT_ENTRY_RESERVED || value == FAT32_FAT_ENTRY_BAD)
            break;

        struct fat32_cluster_chain *chain = (struct fat32_cluster_chain *)kmalloc(sizeof(struct fat32_cluster_chain));
        chain->start = i;

        while (value < FAT32_FAT_ENTRY_EOC && i < bs_info->table_size_32)
            value = fat_table[++i] & 0x0FFFFFFF;

        chain->end = i;
        queue64_push(&fat_q, (uintptr_t)chain);

        dprintk("cluster [%d, %d]\r\n", chain->start, chain->end);
    }

    // create root node and a set that stores the parent folders
    struct fs_node *root = fs_create_folder(bs_info->volume_label, strnlen(bs_info->volume_label, 11), 0);

    struct set64 parent_nodes;
    set64_init(&parent_nodes, 10);

    // read directories
    for (uint32_t i = bs_info->root_cluster; i < fat_q.length; i++)
    {
        struct fat32_cluster_chain *chain = (struct fat32_cluster_chain *)queue64_at(&fat_q, i);

        uint64_t parent_node_ptr;
        struct fs_node *parent_node;
        if (set64_get(&parent_nodes, chain->start, &parent_node_ptr) == 0)
            parent_node = root;
        else
            parent_node = (struct fs_node *)parent_node_ptr;

        if ((parent_node->attrs & FS_NODE_ATTRS_TYPE_MASK) == FS_NODE_ATTRS_TYPE_FILE)
            continue;

        for (uint32_t j = chain->start; j <= chain->end; j++)
        {
            uint32_t sector_offset = j - bs_info->root_cluster;

            dprintk("cluster %d (0x%08x):\r\n", j, (bs_info->first_data_sector + sector_offset) * bs_info->n_bytes_per_sector);

            status = virtio_mmio_read(slot, bs_info->first_data_sector + sector_offset, buff);
            if (status != VIRTIO_BLK_S_OK)
            {
                dprintk("[virtio_mmio@%x] virtio_mmio_read() returned %i!\r\n", VIRTIO_MMIO_ADDR(slot), status);
                kfree(buff);
                kfree(fat_table);
                kfree(bs_info);
                return NULL;
            }

            if (fat32_read_cluster(bs_info, buff, parent_node, &parent_nodes) != 0)
                break;
        }
    }

    // clean ptrs
    for (uint32_t i = bs_info->root_cluster; i < fat_q.length; i++)
        kfree((void *)queue64_at(&fat_q, i));

    queue64_destroy(&fat_q);
    set64_destroy(&parent_nodes);
    kfree(fat_table);
    kfree(buff);
    kfree(bs_info);

    return root;
}
