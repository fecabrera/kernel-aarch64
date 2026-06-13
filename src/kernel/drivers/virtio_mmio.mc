/**
 * Scans all 32 virtio MMIO slots, initializes each valid device found (modern version, non-zero
 * device ID), reads its feature words, and registers its IRQ handler with the GIC. Must be called
 * after gic_init and irq_init, and before irq_enable.
 */
@extern fn virtio_mmio_init();

/**
 * Scans initialized slots after `start` and returns the index of the first slot whose device_id
 * matches. Pass -1 to start from slot 0. Useful for iterating all devices of a given type when
 * multiple are present.
 *
 * @param device_id: device type to search for (see VIRTIO_DEVICE_ID_*)
 * @param start:     slot index to start after (exclusive); pass -1 to begin at 0
 *
 * @return slot index of the first matching device, or -1 if none found
 */
@extern fn virtio_mmio_find_next_slot(device_id: uint32, start: int32) -> int8;

/**
 * Submits a synchronous read request to the virtio-blk device at the given slot. Builds a
 * 3-descriptor chain (request header → data buffer → status), posts it to the available ring, kicks
 * the device via QUEUE_NOTIFY, then spins via wfi() until the device writes the status byte.
 *
 * @param slot:          virtio MMIO slot index
 * @param sector_number: 512-byte sector to read from
 * @param data:          output buffer of at least VIRTIO_MMIO_BLK_SECTOR_SIZE bytes
 *
 * @return VIRTIO_BLK_S_OK (0) on success, VIRTIO_BLK_S_IOERR or VIRTIO_BLK_S_UNSUPP on device
 *         error, VIRTIO_MMIO_INVALID_DEVICE if the slot has no initialized device
 */
@extern fn virtio_mmio_read(slot: int8, sector_number: uint64, data: uint8*) -> int32;