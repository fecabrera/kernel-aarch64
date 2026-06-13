import "memory";
import "io/module";
import "drivers/virtio_mmio";

@extern fn dprintk(fmt: uint8*, ...);
@extern fn sprintf(buffer: uint8*, fmt: uint8*, ...);

@static
let _next: uint8 = 'a';

/**
 * Scans all virtio MMIO slots for block devices and registers each one as an I/O module named
 * "sda", "sdb", etc. (alphabetical, in slot order), creating a corresponding /dev/sd<letter> node.
 * Must be called after io_init().
 */
fn storage_init() {
    let slot: int8 = -1;
    while (true) {
        slot = virtio_mmio_find_next_slot(VIRTIO_DEVICE_ID_BLOCK, slot);
        if(slot == -1)
            break;

        let mod_name: uint8[4];
        set_bytes(mod_name, 0, 4);
        sprintf(mod_name, "sd%c", _next);
        _next = _next + 1;

        dprintk("[storage] adding \"/dev/%s\"\r\n", mod_name);
        io_register_module(mod_name, slot as uint64, storage_read, storage_write);
    }
}

/**
 * I/O read handler for a virtio block device. Reads all sectors spanning [offset, offset+count) via
 * virtio_mmio_read into a temporary buffer, then copies exactly count bytes at the correct
 * intra-sector alignment into buffer.
 *
 * @param buffer:   output buffer (must hold at least count bytes)
 * @param count:    number of bytes to read
 * @param offset:   byte offset into the device (does not need to be sector-aligned)
 * @param slot:     virtio slot index (set by storage_init via io_register_module)
 *
 * @return count on success, negative error code from virtio_mmio_read on failure
 */
@extern fn storage_read(buffer: uint8*, count: uint64, offset: uint64, slot: uint64) -> int32;

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
@extern fn storage_write(buffer: uint8*, count: uint64, offset: uint64, slot: uint64) -> int32;
