#include "storage.h"
#include <debug.h>
#include <io/module.h>
#include <drivers/virtio_mmio.h>

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

int storage_read(uint8_t *buffer, size_t count, size_t offset, uint64_t drv_info)
{
    printk("[/dev/virtio] read(): buff=0x%08x, count=%d, offset=%d, drv_info=%d\r\n", buffer, count, offset, drv_info);

    return 0;
}

int storage_write(uint8_t *buffer, size_t count, size_t offset, uint64_t drv_info)
{
    printk("[/dev/virtio] write(): buff=0x%08x, count=%d, offset=%d, drv_info=%d\r\n", buffer, count, offset, drv_info);

    return 0;
}