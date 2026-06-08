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
 * I/O read handler for a virtio block device. drv_info carries the virtio
 * slot index passed at registration time.
 *
 * @param buffer:   output buffer
 * @param count:    number of bytes to read
 * @param offset:   byte offset into the device to read from
 * @param drv_info: virtio slot index (set by storage_init via io_register_module)
 *
 * @return 0 on success
 */
int storage_read(uint8_t *buffer, size_t count, size_t offset, uint64_t drv_info);

/**
 * I/O write handler for a virtio block device. drv_info carries the virtio
 * slot index passed at registration time.
 *
 * @param buffer:   input buffer
 * @param count:    number of bytes to write
 * @param offset:   byte offset into the device to write to
 * @param drv_info: virtio slot index (set by storage_init via io_register_module)
 *
 * @return 0 on success
 */
int storage_write(uint8_t *buffer, size_t count, size_t offset, uint64_t drv_info);
