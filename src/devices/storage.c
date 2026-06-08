#include "storage.h"
#include <debug.h>
#include <io/module.h>
#include <drivers/virtio_mmio.h>
#include <mm/heap.h>

void storage_init()
{
    virtio_slot_t slot = -1;
    while ((slot = virtio_mmio_find_next_slot(VIRTIO_DEVICE_ID_BLOCK, slot)) != -1)
    {
        char mod_name[50] = {0};
        sprintf(mod_name, "sd%d", slot);

        printk("[storage] adding \"/dev/%s\"\r\n", mod_name);
        io_register_module(mod_name, slot, &storage_read, &storage_write);
    }
}

int storage_read(uint8_t *buffer, size_t count, size_t offset, uint64_t slot)
{
    printk("[/dev/sd%d] read(): buff=0x%08x, count=%d, offset=%d\r\n", slot, buffer, count, offset);

    uint8_t *_buffer = (uint8_t *)kmalloc(VIRTIO_MMIO_BLK_SECTOR_SIZE);

    // read boot sector
    uint64_t sector = offset / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    if (virtio_mmio_read(slot, sector, _buffer) < 0)
    {
        kfree(_buffer);
        return -1;
    }

    memcpy(buffer, _buffer, VIRTIO_MMIO_BLK_SECTOR_SIZE);
    kfree(_buffer);

    return VIRTIO_MMIO_BLK_SECTOR_SIZE;
}

int storage_write(uint8_t *buffer, size_t count, size_t offset, uint64_t slot)
{
    printk("[/dev/sd%d] write(): buff=0x%08x, count=%d, offset=%d\r\n", slot, buffer, count, offset);

    return 0;
}