#pragma once

/**
 * Scans all virtio MMIO slots for block devices and registers each one as an I/O module named
 * "sda", "sdb", etc. (alphabetical, in slot order), creating a corresponding /dev/sd<letter> node.
 * Must be called after io_init().
 */
void storage_init();