#pragma once

#include <stdlib.h>
#include <string.h>

/**
 * Scans all virtio MMIO slots for block devices and registers each one as an
 * I/O module named "sd<slot>" (e.g. "sd0"), creating a corresponding
 * /dev/sd<slot> node. Must be called after io_init().
 */
void storage_init();

/**
 * I/O read handler for a virtio block device. Reads the sector containing
 * offset via virtio_mmio_read and copies it into buffer.
 *
 * @param buffer:   output buffer (must hold at least VIRTIO_MMIO_BLK_SECTOR_SIZE bytes)
 * @param count:    number of bytes requested
 * @param offset:   byte offset into the device; determines which sector is read
 * @param slot:     virtio slot index (set by storage_init via io_register_module)
 *
 * @return VIRTIO_MMIO_BLK_SECTOR_SIZE on success, -1 on read failure
 */
int storage_read(uint8_t *buffer, size_t count, size_t offset, uint64_t slot);

/**
 * I/O write handler for a virtio block device.
 *
 * @param buffer:   input buffer
 * @param count:    number of bytes to write
 * @param offset:   byte offset into the device to write to
 * @param slot:     virtio slot index (set by storage_init via io_register_module)
 *
 * @return 0 on success
 */
int storage_write(uint8_t *buffer, size_t count, size_t offset, uint64_t slot);
