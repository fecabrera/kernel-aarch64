import "debug";
import "mm";
import "memory";
import "io";
import "libc/stdio";
import "interrupts/drivers/virtio_mmio";

@static
let _next: char = 'a';

/**
 * Scans all virtio MMIO slots for block devices and registers each one as an I/O module named
 * "sda", "sdb", etc. (alphabetical, in slot order), creating a corresponding /dev/sd<letter> node.
 * Must be called after io_init().
 */
fn storage_init() {
    let slot: int8 = -1;
    while (true) {
        slot = virtio_mmio_find_next_slot(VIRTIO_DEVICE_ID_BLOCK, slot);
        if (slot == -1)
            break;

        let mod_name: char[4];
        set_bytes(mod_name, 0, 4);
        sprintf(mod_name, "sd%c", _next);
        _next = _next + 1;

        dprintk("[storage] adding \"/dev/%s\"\n", mod_name);

        let attrs: uint32 = node_attrs::READ;
        io_register_module(mod_name, attrs, slot as uint64, storage_read, storage_write);
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
fn storage_read(buffer: uint8*, count: uint64, offset: uint64, slot: uint64) -> int64 {
    dprintk("[/dev/sd%d] buf=0x%p, count=%llu, offset=%llu\n", slot, buffer, count, offset);

    let first_sector: uint64 = offset / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    let last_sector: uint64 = (offset + count - 1) / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    let sectors_to_read: uint64 = last_sector - first_sector + 1;

    dprintk("[/dev/sd%d] first_sector=%d, last_sector=%d, sectors_to_read=%d\n",
            slot, first_sector, last_sector, sectors_to_read);

    // allocate temporary buffer
    let tmp: uint8* = kalloc<uint8>(sectors_to_read * VIRTIO_MMIO_BLK_SECTOR_SIZE);
    defer kdealloc(tmp);

    // read boot sectors
    let r = struct range { end = sectors_to_read };
    for i in &r {
        // calculate sector to read
        let sector: uint64 = first_sector + i;

        // calculate were in the temporary buffer we need to store
        let buf_offset: uint64 = i * VIRTIO_MMIO_BLK_SECTOR_SIZE;

        // read sector
        let data = (tmp as uint64 + buf_offset) as uint8*;
        let n = virtio_mmio_read(slot as int8, sector, data);
        if (n < 0) {
            // pass error n to caller
            return n;
        }
    }

    // copy data requested to temporary buffer
    bytecopy(buffer, (tmp as uint64 + (offset % VIRTIO_MMIO_BLK_SECTOR_SIZE)) as uint8*, count);

    // return bytes delivered to the caller, not raw sectors read
    return count as int64;
}

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
fn storage_write(buffer: uint8*, count: uint64, offset: uint64, slot: uint64) -> int64 {
    let first_sector: uint64 = offset / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    let last_sector: uint64 = (offset + count - 1) / VIRTIO_MMIO_BLK_SECTOR_SIZE;
    let sectors_to_read: uint64 = last_sector - first_sector + 1;

    dprintk("[/dev/sd%d] first_sector=%d, last_sector=%d, sectors_to_read=%d\n",
            slot, first_sector, last_sector, sectors_to_read);

    return 0;
}
