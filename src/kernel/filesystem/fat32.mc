import "debug";
import "memory";
import "cpu";
import "fs";
import "vfs";

// Partition type byte
const MBR_PARTITION_TYPE_EMPTY = 0;
const MBR_PARTITION_TYPE_NTFS = 0x07;
const MBR_PARTITION_TYPE_FAT32_CHS = 0x0B;
const MBR_PARTITION_TYPE_FAT32_LBA = 0x0C;
const MBR_PARTITION_TYPE_EXTENDED = 0x0F;
const MBR_PARTITION_TYPE_LINUX_SWAP = 0x82;
const MBR_PARTITION_TYPE_LINUX_FILESYSTEM = 0x83;

// Media descriptor byte — stored in BPB_Media and FAT[0] low byte
const MBR_MEDIA_DESCRIPTOR_REMOVABLE = 0xF0; // 3.5" / 5.25" removable disk
const MBR_MEDIA_DESCRIPTOR_FIXED = 0xF8;     // fixed (non-removable) disk
const MBR_MEDIA_DESCRIPTOR_F0_ALT = 0xF9;    // double-sided, 80 tracks, 15 sectors
const MBR_MEDIA_DESCRIPTOR_FC = 0xFC;        // single-sided, 40 tracks, 9 sectors
const MBR_MEDIA_DESCRIPTOR_FD = 0xFD;        // double-sided, 40 tracks, 9 sectors
const MBR_MEDIA_DESCRIPTOR_FE = 0xFE;        // single-sided, 40 tracks, 8 sectors
const MBR_MEDIA_DESCRIPTOR_FF = 0xFF;        // double-sided, 40 tracks, 8 sectors

// ── Directory entry attributes ────────────────────────────────────────────────

const FAT32_ATTR_READ_ONLY = (1 << 0); // file is read-only
const FAT32_ATTR_HIDDEN = (1 << 1);    // file is hidden
const FAT32_ATTR_SYSTEM = (1 << 2);    // file is a system file
const FAT32_ATTR_VOLUME_ID = (1 << 3); // entry is a volume label
const FAT32_ATTR_DIRECTORY = (1 << 4); // entry is a subdirectory
const FAT32_ATTR_ARCHIVE = (1 << 5);   // file has been modified since last backup
// combination used to mark a long file name entry
const FAT32_ATTR_LFN = (FAT32_ATTR_READ_ONLY | FAT32_ATTR_HIDDEN | FAT32_ATTR_SYSTEM | FAT32_ATTR_VOLUME_ID);

// ── Special first-byte values for dir_entry.name[0] ──────────────────────────

const FAT32_DIRENT_FREE = 0xE5; // entry was deleted (slot is free)
const FAT32_DIRENT_END = 0x00;  // no more entries in this directory
const FAT32_DIRENT_DOT = 0x2E;  // "." or ".." entry

// ── FAT entry values ──────────────────────────────────────────────────────────

const FAT32_FAT_ENTRY_FREE = 0x00000000;     // cluster is free
const FAT32_FAT_ENTRY_RESERVED = 0x00000001; // reserved, do not use
const FAT32_FAT_ENTRY_BAD = 0x0FFFFFF7;      // cluster is bad
const FAT32_FAT_ENTRY_EOC = 0x0FFFFFF8;      // end of chain (any value >= this is EOC)

// ── LFN sequence number flags ─────────────────────────────────────────────────

const FAT32_LFN_LAST_ENTRY = 0x40; // OR'd into lfn_entry.order for the last (highest) LFN entry

/**
 * FAT32 extended boot record (EBR), occupies bytes 36–61 of the
 * FAT32 volume boot sector, immediately after the common BPB fields.
 * All multi-byte fields are little-endian.
 */
@packed
struct fat32_ext_bs {
    table_size_32: uint32;    // sectors per FAT (FAT32 only)
    extended_flags: uint16;   // mirroring flags; bit 7: single active FAT, bits 3:0: active FAT number
    fat_version: uint16;      // FAT32 version (high byte = major, low = minor; typically 0x0000)
    root_cluster: uint32;     // cluster number of the root directory (usually 2)
    fat_info: uint16;         // sector number of the FSInfo structure
    backup_bs_sector: uint16; // sector number of the backup boot sector (usually 6)
    reserved_0: uint8[12];
    drive_number: uint8;      // BIOS drive number (0x00 = floppy, 0x80 = hard disk)
    reserved_1: uint8;
    boot_signature: uint8;    // extended boot signature (0x29 = volume_id/label/type fields present)
    volume_id: uint32;        // volume serial number
    volume_label: uint8[11];  // volume label, padded with spaces (not null-terminated)
    fat_type_label: uint8[8]; // always "FAT32   " (informational only, not for type detection)
}

/**
 * Master Boot Record boot sector (sector 0 of the disk).
 * Contains the BIOS Parameter Block (BPB), bootstrap code, and the 512-byte boot sector signature.
 * All multi-byte fields are little-endian. Use memcpy to read uint16_t/uint32_t fields to avoid
 * alignment faults.
 */
@packed
struct mbr_boot_sector {
    mbr_jump_boot: uint8[3];      // x86 jump instruction to bootstrap code
    oem_name: uint8[8];           // OEM name string, NOT null-terminated (e.g. "mkfs.fat")
    n_bytes_per_sector: uint16;   // bytes per sector (typically 512)
    n_sectors_per_cluster: uint8; // sectors per allocation cluster (power of 2)
    n_reserved_sectors: uint16;   // reserved sectors before first FAT (includes this sector)
    n_fat: uint8;                 // number of FAT copies (usually 2)
    n_root_dir_entries: uint16;   // root directory entries (0 for FAT32)
    total_sectors_16: uint16;     // total sectors if < 65536, else 0 (use total_sectors_32)
    media_dtor_type: uint8;       // media descriptor byte (see MBR_MEDIA_DESCRIPTOR_*)
    table_size_16: uint16;        // sectors per FAT (FAT12/16 only; 0 for FAT32)
    n_sectors_per_track: uint16;  // sectors per track (CHS geometry)
    n_heads: uint16;              // number of heads (CHS geometry)
    n_hidden_sectors: uint32;     // hidden sectors preceding this partition
    total_sectors_32: uint32;     // total sectors if >= 65536 (FAT32 always uses this)
    boot_code: uint8[474];        // bootstrap code + EBR (for FAT32, EBR starts at offset 36)
    mbr_signature_word: uint16;   // boot sector signature (must be 0xAA55)
}

/**
 * FAT32 8.3 short-name directory entry (32 bytes).
 * All multi-byte fields are little-endian.
 * Use memcpy when accessing uint16_t/uint32_t fields through a pointer.
 */
@packed
struct fat32_dir_entry {
    name: uint8[11];       // short name: 8 chars name + 3 chars extension, space-padded
    attributes: uint8;     // FAT32_ATTR_* flags
    reserved: uint8;       // reserved for Windows NT
    create_time_cs: uint8; // creation time, 10ms units (0–199)
    create_time: uint16;   // creation time: bits[15:11]=hour, [10:5]=minute, [4:0]=second/2
    create_date: uint16;   // creation date: bits[15:9]=year-1980, [8:5]=month, [4:0]=day
    access_date: uint16;   // last access date (same format as create_date)
    cluster_high: uint16;  // high 16 bits of first cluster (FAT32 only)
    modify_time: uint16;   // last modified time (same format as create_time)
    modify_date: uint16;   // last modified date (same format as create_date)
    cluster_low: uint16;   // low 16 bits of first cluster
    file_size: uint32;     // file size in bytes (0 for directories)
}

/**
 * FAT32 long file name (LFN) directory entry (32 bytes).
 * LFN entries precede their corresponding 8.3 entry in reverse order.
 * Name characters are UTF-16LE; unused trailing chars are 0xFFFF.
 */
@packed
struct fat32_lfn_entry {
    order: uint8;      // sequence number (1-based); OR'd with FAT32_LFN_LAST_ENTRY if last
    name1: uint16[5];  // UTF-16LE name characters 1–5
    attributes: uint8; // always FAT32_ATTR_LFN (0x0F)
    type: uint8;       // always 0
    checksum: uint8;   // checksum of the 8.3 short name
    name2: uint16[6];  // UTF-16LE name characters 6–11
    cluster: uint16;   // always 0x0000
    name3: uint16[2];  // UTF-16LE name characters 12–13
}

struct fat32_bs_info {
    // fields from MBR boot sector
    volume_label: uint8[12]; // volume label from EBR, null-terminated
    n_fat: uint8;
    n_sectors_per_cluster: uint8;
    n_reserved_sectors: uint16;
    n_bytes_per_sector: uint16;
    total_sectors_16: uint16;
    total_sectors_32: uint32;
    // fields from FAT32 extended boot record
    drive_number: uint8;
    root_cluster: uint32;
    table_size_32: uint32;
    // derived fields
    first_fat_sector: uint32;
    first_data_sector: uint32;
    total_sectors: uint32;
    data_sectors: uint32;
    total_clusters: uint32;
    fat_table: uint32*; // full FAT table loaded at mount time; kept alive for cluster
                        // chain traversal at read time
    n_fat_entries: uint32;
}

// Locates a directory entry on disk: the cluster it lives in and its index within that cluster's
// sector buffer (0-based, relative to the sector start).
struct fat32_entry_reference {
    cluster: uint32; // cluster number of the directory containing this entry
    offset: uint32;  // index of the 8.3 dir entry within the cluster's sector buffer (0-based)
    n_lfn_entries: uint32; // number of LFN entries preceding the 8.3 entry (0 if short name only)
}

/**
 * Prints all BPB fields of the MBR boot sector to the kernel log via printk.
 * Reads multi-byte fields with memcpy to avoid alignment faults.
 *
 * @param boot_sector: pointer to a packed MBR boot sector read from disk
 */
@extern fn fat32_dump_boot_sector(boot_sector: struct mbr_boot_sector*);

/**
 * Prints all fields of a FAT32 extended boot record to the kernel log.
 * Reads multi-byte fields with memcpy to avoid alignment faults.
 *
 * @param ext_br: pointer to the EBR, typically at &boot_sector->boot_code[0]
 */
@extern fn fat32_dump_extended_boot_record(ext_br: struct fat32_ext_bs*);

/**
 * Prints all fields of an 8.3 directory entry to the kernel log.
 * Reads multi-byte fields with memcpy to avoid alignment faults.
 *
 * @param dir_entry: pointer to a packed fat32_dir_entry
 */
@extern fn fat32_dump_dir_entry(dir_entry: struct fat32_dir_entry*);

/**
 * Prints all fields of a long file name directory entry to the kernel log.
 *
 * @param lfn_dir_entry: pointer to a packed fat32_lfn_entry
 */
@extern fn fat32_dump_lfn_entry(lfn_dir_entry: struct fat32_lfn_entry*);

/**
 * Checks whether a 512-byte sector buffer contains a valid FAT32 boot sector.
 * Verifies the boot signature (0xAA55), and that the EBR fields table_size_32 and root_cluster are
 * non-zero.
 *
 * @param buff: 512-byte sector buffer to inspect
 *
 * @return non-zero if the buffer looks like a valid FAT32 boot sector, zero otherwise
 */
@extern fn fat32_is_boot_sector(buff: uint8*) -> int32;

/**
 * Parses a raw 512-byte boot sector buffer into a fat32_bs_info struct.
 * Reads BPB fields from mbr_boot_sector and EBR fields from the embedded fat32_ext_bs using memcpy
 * to avoid alignment faults. Also computes derived fields: first_fat_sector, first_data_sector,
 * data_sectors, total_clusters. Copies the EBR volume label into bs_info->volume_label,
 * null-terminated and with trailing spaces stripped.
 *
 * @param buff:    512-byte buffer containing the boot sector read from disk
 * @param bs_info: output struct populated with parsed and derived fields
 */
@extern fn fat32_parse_boot_sector(buff: uint8*, bs_info: struct fat32_bs_info*);

/**
 * Parses one pre-read FAT sector into bs_info->fat_table, applying _le32 and masking to 28 bits per
 * the spec. Writes entries starting at bs_info->fat_table[sector_offset * n_entries_per_sector].
 * Each entry holds the raw next-cluster value; compare against FAT32_FAT_ENTRY_* to interpret
 * (free, bad, EOC, or next cluster number). bs_info->fat_table must be allocated before calling.
 * Call once per FAT sector, in order, to populate the full table.
 *
 * @param bs_info:       parsed boot sector info (fat_table must be allocated; provides sector/entry
 * sizes)
 * @param buff:          512-byte buffer containing the FAT sector already read from disk
 * @param sector_offset: index of this sector within the FAT (0-based)
 *
 * @return number of entries written into bs_info->fat_table
 */
@extern fn fat32_read_fat_table(bs_info: struct fat32_bs_info*, buff: uint8*, sector_offset: uint32) -> uint32;

/**
 * Scans bs_info->fat_table and pushes the starting cluster number of each distinct chain into
 * cluster_chains. Iterates from root_cluster to n_fat_entries, marking visited clusters so each
 * chain is enqueued exactly once. FREE, RESERVED, and BAD entries are skipped.
 *
 * @param bs_info:        parsed boot sector info (fat_table and n_fat_entries must be set)
 * @param cluster_chains: output queue; receives one uint32_t per chain (the start cluster)
 */
@extern fn fat32_build_cluster_chains(bs_info: struct fat32_bs_info*, cluster_chains: uint8* /* struct queue32* */);

/**
 * Mounts a FAT32 volume accessible at device_path. Reads the boot sector via vfs_read, validates
 * the FAT32 signature, parses the BPB, creates or resolves the VFS directory, registers a
 * mountpoint, reads the full FAT table, then recursively traverses all directories via
 * _fat32_read_cluster.
 *
 * If mountpoint is NULL, the volume is mounted at "/volumes/<label>" (directory created
 * automatically). If mountpoint is non-NULL, the existing VFS node at that path is used.
 *
 * @param device_path: VFS path to the block device (e.g. "/dev/sda")
 * @param mountpoint:  VFS path to mount at, or NULL to auto-derive from the volume label
 *
 * @return 0 on success; -1 I/O error; -2 not a valid FAT32 volume; -3 vfs_create_dir failed;
 *         -4 vfs_get_node_for_path failed; -5 vfs_create_mountpoint failed;
 *         -6 FAT table read error; -7 fs tree build error
 */
@extern fn fat32_mount(device_path: uint8*, mountpoint: uint8*) -> int32;

/**
 * Unmounts the FAT32 volume mounted at the VFS path derived from pathname.
 * Frees bs_info->fat_table, then destroys the mountpoint via vfs_destroy_mountpoint.
 * Not yet implemented.
 *
 * @param device_path: VFS path to the block device used when mounting (e.g. "/dev/sda")
 *
 * @return 0 on success, -1 on error
 */
fn fat32_unmount(device_path: uint8*) -> int32 {
    // steps:
    //   1. get vfs_mount
    //   2. get bs_info
    //   3. kfree(bs_info->fat_table)
    //   4. unmount fs_mount
    return -1;
}

/**
 * vfs_handler_t read handler for FAT32 mountpoints.
 * Reads count bytes from the file described by node->info (fat32_entry_reference) into buffer,
 * starting at offset.
 *
 * @param node:   fs_node whose info points to a fat32_entry_reference
 * @param buffer: output buffer
 * @param count:  number of bytes to read
 * @param offset: byte offset into the file
 *
 * @return number of bytes read on success, negative on error
 */
fn fat32_read(node: struct fs_node*, buffer: uint8*, count: uint64, offset: uint64) -> int32 {
    let entry_ref = node->info as struct fat32_entry_reference*;
    let fs_mp = node->mount;
    let bs_info = fs_mp->info as struct fat32_bs_info*;

    dprintk("[fat32] node: entry_ref=0x%08X, fs_mp=0x%08X, bs_info=0x%08X\r\n", entry_ref, fs_mp,
            bs_info);
    dprintk("[fat32] fs_mp: mp=\"%s\", device=\"%s\"\r\n", fs_mp->mountpoint, fs_mp->device);
    dprintk("[fat32] entry_ref: cluster=%d, offset=%d, n_lfn_entries=%d\r\n", entry_ref->cluster,
            entry_ref->offset, entry_ref->n_lfn_entries);

    // read fat32_dir_entry
    let dir_entry_sector: uint32 = bs_info->first_data_sector + (entry_ref->cluster - bs_info->root_cluster);
    let dir_entry_size: uint32 = sizeof(struct fat32_dir_entry) as uint32;
    let dir_entry_offset: uint32 = dir_entry_sector * bs_info->n_bytes_per_sector as uint32 +
                                (entry_ref->offset + entry_ref->n_lfn_entries) * dir_entry_size;
    let dir_entry = alloc<uint8>(dir_entry_size as uint64) as struct fat32_dir_entry*;
    defer dealloc(dir_entry);

    dprintk("[fat32] count=%d, offset=0x%08X\r\n", dir_entry_size, dir_entry_offset);
    let status = vfs_read(fs_mp->device, dir_entry as uint8*, dir_entry_size as uint64, dir_entry_offset as uint64);
    if (status < 0) {
        dprintk("[fat32] vfs_read() returned %d!\r\n", status);
        return status;
    }

    // parse data_cluster and file_size
    let cluster_low: uint16;
    let cluster_high: uint16;
    let file_size: uint32;
    copy_bytes(&cluster_low, &dir_entry->cluster_low, 1);
    copy_bytes(&cluster_high, &dir_entry->cluster_high, 1);
    copy_bytes(&file_size, &dir_entry->file_size, 1);

    let data_cluster: uint32 = (le16(cluster_high) as uint32 << 16) | (le16(cluster_low) as uint32);
    dprintk("[fat32] cluster=%d, file_size=%d\r\n", data_cluster, file_size);

    // validate offset
    if (file_size as uint64 <= offset) {
        dprintk("[fat32] offset must be less than the file size!\r\n");
        return -1;
    }

    // calculate which and how many sectors to read
    // total sectors used by the file contents
    let total_data_sectors: uint32 = file_size / (bs_info->n_bytes_per_sector as uint32);
    // first data sector of the file contents to read
    let first_data_sector_to_read: uint64 = offset / (bs_info->n_bytes_per_sector as uint64);
    // last data sector of the file contents to read
    let last_data_sector_to_read: uint64 = (offset + count - 1) / (bs_info->n_bytes_per_sector as uint64);
    let n_data_sectors_to_read: uint64 = last_data_sector_to_read - first_data_sector_to_read + 1;

    dprintk("[fat32] total_data_sectors=%d, first_data_sector_to_read=%d, last_data_sector_to_read=%d, n_data_sectors_to_read=%d\r\n",
            total_data_sectors, first_data_sector_to_read, last_data_sector_to_read, n_data_sectors_to_read);

    // follow fat_table
    let cluster_to_read: uint32 = data_cluster;
    {
        let i: uint64 = 0;
        while (i < first_data_sector_to_read) {
            cluster_to_read = bs_info->fat_table[cluster_to_read];
            i = i + 1;
        }
    }

    // read contents
    let tmp: uint8* = alloc<uint8>(n_data_sectors_to_read * (bs_info->n_bytes_per_sector as uint64));
    defer dealloc(tmp);
    {
        let i: uint64 = 0;
        while (i < n_data_sectors_to_read) {
            // first data sector of the file contents
            let data_sector: uint32 = bs_info->first_data_sector + (cluster_to_read - bs_info->root_cluster);
            let data_offset: uint32 = data_sector * (bs_info->n_bytes_per_sector as uint32);

            dprintk("[fat32] cluster_to_read=%d, data_sector=%d, data_offset=0x%08X\r\n",
                    cluster_to_read, data_sector, data_offset);
            vfs_read(fs_mp->device, &tmp[i * (bs_info->n_bytes_per_sector as uint64)],
                    bs_info->n_bytes_per_sector as uint64, data_offset as uint64);

            // update cluster_to_read
            cluster_to_read = bs_info->fat_table[cluster_to_read];

            i = i + 1;
        }
    }

    // copy data to buffer
    copy_bytes(buffer, &tmp[offset % (bs_info->n_bytes_per_sector as uint64)], count);

    return 0;
}

/**
 * vfs_handler_t write handler for FAT32 mountpoints. Not yet implemented.
 *
 * @param node:   fs_node whose info points to a fat32_entry_reference
 * @param buffer: input buffer
 * @param count:  number of bytes to write
 * @param offset: byte offset into the file
 *
 * @return number of bytes written on success, negative on error
 */
fn fat32_write(node: struct fs_node*, buffer: uint8*, count: uint64, offset: uint64) -> int32 {
    let entry_ref = node->info as struct fat32_entry_reference*;
    let fs_mp = node->mount;
    let bs_info = fs_mp->info as struct fat32_bs_info*;

    dprintk("[fat32] node: entry_ref=0x%08X, fs_mp=0x%08X, bs_info=0x%08X\r\n", entry_ref, fs_mp,
            bs_info);
    dprintk("[fat32] fs_mp: mp=\"%s\", device=\"%s\"\r\n", fs_mp->mountpoint, fs_mp->device);
    dprintk("[fat32] entry_ref: cluster=%d, offset=%d, n_lfn_entries=%d\r\n", entry_ref->cluster,
            entry_ref->offset, entry_ref->n_lfn_entries);

    return -1;
}