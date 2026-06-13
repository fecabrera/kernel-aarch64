#include "storage.h"
#include <debug.h>
#include <drivers/virtio_mmio.h>
#include <io/module.h>
#include <mm/heap.h>
#include <stdio.h>

// static char _next = 'a';

// void storage_init() {
//     virtio_slot_t slot = -1;
//     while ((slot = virtio_mmio_find_next_slot(VIRTIO_DEVICE_ID_BLOCK, slot)) != -1) {
//         char mod_name[50] = {0};
//         sprintf(mod_name, "sd%c", _next++);

//         dprintk("[storage] adding \"/dev/%s\"\r\n", mod_name);
//         io_register_module(mod_name, slot, &storage_read, &storage_write);
//     }
// }

int storage_read(uint8_t *buffer, size_t count, size_t offset, uint64_t slot) {
    uint64_t first_sector = offset / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    uint64_t last_sector = (offset + count - 1) / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    uint64_t sectors_to_read = last_sector - first_sector + 1;

    dprintk("[/dev/sd%d] first_sector=%d, last_sector=%d, sectors_to_read=%d\r\n", slot,
            first_sector, last_sector, sectors_to_read);

    // allocate temporary buffer
    uint8_t *tmp = (uint8_t *)kmalloc(sectors_to_read * VIRTIO_MMIO_BLK_SECTOR_SIZE);

    // read boot sectors
    for (uint64_t i = 0; i < sectors_to_read; i++) {
        // calculate sector to read
        uint64_t sector = first_sector + i;

        // calculate were in the temporary buffer we need to store
        size_t buf_offset = i * VIRTIO_MMIO_BLK_SECTOR_SIZE;

        // read sector
        int status = virtio_mmio_read(slot, sector, tmp + buf_offset);
        if (status < 0) {
            // free temporary buffer
            kfree(tmp);

            // pass error status to caller
            return status;
        }
    }

    // copy data requested to temporary buffer
    memcpy(buffer, tmp + (offset % VIRTIO_MMIO_BLK_SECTOR_SIZE), count);

    // free temporary buffer
    kfree(tmp);

    return count;
}

int storage_write(uint8_t *buffer, size_t count, size_t offset, uint64_t slot) {
    uint64_t first_sector = offset / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    uint64_t last_sector = (offset + count - 1) / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    uint64_t sectors_to_read = last_sector - first_sector + 1;

    dprintk("[/dev/sd%d] first_sector=%d, last_sector=%d, sectors_to_read=%d\r\n", slot,
            first_sector, last_sector, sectors_to_read);

    return 0;
}