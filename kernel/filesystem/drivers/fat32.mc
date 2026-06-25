import "debug";
import "memory";
import "cpu";
import "utf16";
import "libc/string";
import "filesystem/fs";
import "filesystem/vfs";

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

const FAT32_MOUNT_ERROR_CANNOT_READ_BOOT_SECTOR = -1;
const FAT32_MOUNT_ERROR_VOLUME_IS_NOT_FAT32 = -2;
const FAT32_MOUNT_ERROR_MOUNTPOINT_NOT_FOUND = -3;
const FAT32_MOUNT_ERROR_MOUNTPOINT_IS_NOT_A_FOLDER = -4;
const FAT32_MOUNT_ERROR_CANNOT_CREATE_VFS_MOUNT = -5;
const FAT32_MOUNT_ERROR_CANNOT_READ_FAT_TABLE = -6;
const FAT32_MOUNT_ERROR_CANNOT_BUILD_FS_TREE = -7;

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
 * Checks whether a 512-byte sector buffer contains a valid FAT32 boot sector.
 * Verifies the boot signature (0xAA55), and that the EBR fields table_size_32 and root_cluster are
 * non-zero.
 *
 * @param buff: 512-byte sector buffer to inspect
 *
 * @return non-zero if the buffer looks like a valid FAT32 boot sector, zero otherwise
 */
fn fat32_is_boot_sector(buf: uint8*) -> bool {
    let bs = buf as struct mbr_boot_sector*;

    // check boot sector signature
    let signature: uint16; 
    bytecopy(&signature, &bs->mbr_signature_word, 1);

    if (le16(signature) != 0xAA55)
        return false;

    // check FAT32-specific fields in the extended boot record
    let ext_br = bs->boot_code as struct fat32_ext_bs*;
    if (ext_br->table_size_32 == 0 or ext_br->root_cluster == 0)
        return false;

    return true;
}

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
fn fat32_parse_boot_sector(buff: uint8*, bs_info: struct fat32_bs_info*) {
    let bs = buff as struct mbr_boot_sector*;
    let ext_br = bs->boot_code as struct fat32_ext_bs*;

    // extract volume label from EBR, which is more likely to be human-friendly than the OEM name in
    // the MBR boot sector. Also trim trailing spaces.
    strntrimend(bs_info->volume_label, ext_br->volume_label as uint8*, 11);
    
    // ARM doesn't like unaligned memory access, so we need to memcpy the fields into local
    // variables before using them
    let n_fat: uint8;
    let n_sectors_per_cluster: uint8;
    let n_reserved_sectors: uint16;
    let n_bytes_per_sector: uint16;
    let total_sectors_16: uint16;
    let total_sectors_32: uint32;
    bytecopy(&n_sectors_per_cluster, &bs->n_sectors_per_cluster, 1);
    bytecopy(&n_fat, &bs->n_fat, 1);
    bytecopy(&n_reserved_sectors, &bs->n_reserved_sectors, 1);
    bytecopy(&n_bytes_per_sector, &bs->n_bytes_per_sector, 1);
    bytecopy(&total_sectors_16, &bs->total_sectors_16, 1);
    bytecopy(&total_sectors_32, &bs->total_sectors_32, 1);

    // extended boot record fields
    let drive_number: uint8; 
    let root_cluster: uint32;
    let table_size_32: uint32;
    bytecopy(&drive_number, &ext_br->drive_number, 1);
    bytecopy(&root_cluster, &ext_br->root_cluster, 1);
    bytecopy(&table_size_32, &ext_br->table_size_32, 1);

    // derived fields
    let total_sectors = total_sectors_16 == 0 ? total_sectors_32 : total_sectors_16 as uint32;
    let first_fat_sector = n_reserved_sectors as uint32;
    let first_data_sector = n_reserved_sectors + (table_size_32 * n_fat);
    let data_sectors = total_sectors - first_data_sector;
    let total_clusters = data_sectors / n_sectors_per_cluster;

    // fill the bs_info struct
    bs_info->n_fat = n_fat;
    bs_info->n_sectors_per_cluster = n_sectors_per_cluster;
    bs_info->n_reserved_sectors = n_reserved_sectors;
    bs_info->n_bytes_per_sector = n_bytes_per_sector;
    bs_info->total_sectors_16 = total_sectors_16;
    bs_info->total_sectors_32 = total_sectors_32;
    bs_info->drive_number = drive_number;
    bs_info->root_cluster = root_cluster;
    bs_info->table_size_32 = table_size_32;
    bs_info->first_fat_sector = first_fat_sector;
    bs_info->first_data_sector = first_data_sector;
    bs_info->total_sectors = total_sectors;
    bs_info->data_sectors = data_sectors;
    bs_info->total_clusters = total_clusters;
}

@private
fn fat32_read_cluster(pathname: uint8*, bs_info: struct fat32_bs_info*, cluster: uint32,
                      parent_node: struct fs_node*, parent_nodes: struct set<uint32, struct fs_node*>*,
                      mount: struct fs_mount*) -> int32 {
    let sector_offset: uint32 = cluster - bs_info->root_cluster;
    let sector: uint32 = bs_info->first_data_sector + sector_offset;

    dprintk("cluster %d (%p):\n", cluster, sector * bs_info->n_bytes_per_sector);
    let n_entries_per_sector: uint16 = bs_info->n_bytes_per_sector / 32;

    // allocate buffer
    let buf: uint8* = alloc<uint8>(bs_info->n_bytes_per_sector as uint64);
    defer dealloc(buf);

    let status = vfs_read(pathname, buf, bs_info->n_bytes_per_sector as uint64,
                          sector as uint64 * bs_info->n_bytes_per_sector);
    if (status < 0) {
        dprintk("[fat32] vfs_read() returned %i!\n", status);
        return -1;
    }

    let first_entry = buf as struct fat32_dir_entry*;
    let offset: uint16 = 0;
    while (offset < n_entries_per_sector) {
        defer offset = offset + 1;
        
        let dir_entry = &first_entry[offset];

        if (dir_entry->name[0] == FAT32_DIRENT_END) return 1;

        if ((dir_entry->name[0] == FAT32_DIRENT_FREE) or
            (dir_entry->name[0] == FAT32_ATTR_SYSTEM) or
            (dir_entry->name[0] == FAT32_ATTR_VOLUME_ID))
            continue;

        dprintk("entry #%d: \n", offset);

        let lfn_entries: struct stack<struct fat32_lfn_entry*>;
        stack_init(&lfn_entries, 10);
        defer stack_destroy(&lfn_entries);

        // copy offset
        let _offset = offset;

        // parse lfn entries
        let attributes: uint8;
        let n_lfn_entries: uint32 = 0;
        while (true) {
            bytecopy(&attributes, &dir_entry->attributes, 1);

            if (attributes != FAT32_ATTR_LFN)
                break;

            n_lfn_entries = n_lfn_entries + 1;
            offset = offset + 1;

            let lfn_entry = dir_entry as struct fat32_lfn_entry*;
            stack_push(&lfn_entries, lfn_entry);

            dir_entry = &dir_entry[1];
        }

        bytecopy(&attributes, &dir_entry->attributes, 1);

        let dir_name: uint8[12];
        set_bytes(dir_name, 0, 12);
        bytecopy(dir_name, dir_entry->name, 11);

        let lfn_dir_name = alloc<uint8>(n_lfn_entries as uint64 * 13 + 1);
        defer dealloc(lfn_dir_name);
        set_bytes(lfn_dir_name, 0, n_lfn_entries as uint64 * 13 + 1);

        let i: uint32 = 0;
        while (i < n_lfn_entries) {
            defer i = i + 1;

            let lfn_entry = stack_pop(&lfn_entries);
            
            let _lfn_dir_name: uint16[14];
            set_bytes(_lfn_dir_name, 0, 14);
            bytecopy(_lfn_dir_name, lfn_entry->name1, 5);
            bytecopy(&_lfn_dir_name[5], lfn_entry->name2, 6);
            bytecopy(&_lfn_dir_name[11], lfn_entry->name3, 2);

            utf16lencpy(&lfn_dir_name[i * 13], _lfn_dir_name, 14);
        }

        let cluster_lo: uint16;
        let cluster_hi: uint16;
        let file_size: uint32;
        bytecopy(&cluster_lo, &dir_entry->cluster_low, 1);
        bytecopy(&cluster_hi, &dir_entry->cluster_high, 1);
        bytecopy(&file_size, &dir_entry->file_size, 1);

        let next_cluster = {
            let _cluster_hi = le16(cluster_hi) as uint32;
            let _cluster_lo = le16(cluster_lo) as uint32;
            emit (_cluster_hi << 16) | _cluster_lo;
        };

        let name: uint8*;
        if (n_lfn_entries) {
            let l = strlen(lfn_dir_name);
            name = alloc<uint8>(l + 1);
            strncpy(name, lfn_dir_name, l + 1);
        } else {
            name = alloc<uint8>(13);
            let name_l = strntrimend(name, dir_name, 8);
            if (strntrimend(&name[name_l + 1], &dir_name[8], 3) > 0)
                name[name_l] = '.';
            else
                name[name_l] = '\0';
        }
        defer dealloc(name);

        if (strncmp(name, ".", 2) != 0 and strncmp(name, "..", 3) != 0) {
            let entry_ref = alloc<struct fat32_entry_reference>(1);
            entry_ref->cluster = cluster;
            entry_ref->offset = _offset as uint32;
            entry_ref->n_lfn_entries = n_lfn_entries;

            let node: struct fs_node*;
            if (attributes & FAT32_ATTR_DIRECTORY) {
                let attrs: uint32 = FS_NODE_ATTRS_PERMISSIONS_READ;
                if ((attributes & FAT32_ATTR_READ_ONLY) == 0)
                    attrs = attrs | FS_NODE_ATTRS_PERMISSIONS_WRITE;

                node = fs_add_subfolder(parent_node, name, attrs, entry_ref, mount);
            } else {
                let file_size: uint32;
                bytecopy(&file_size, &dir_entry->file_size, 1);

                let attrs: uint32 = FS_NODE_ATTRS_PERMISSIONS_READ;
                if ((attributes & FAT32_ATTR_READ_ONLY) == 0)
                    attrs = attrs | FS_NODE_ATTRS_PERMISSIONS_WRITE;

                node = fs_add_file_to_folder(parent_node, name, file_size as uint64,
                                             attrs, entry_ref, mount);
            }

            set_set(parent_nodes, next_cluster, node);
        }

        dprintk("  name          = \"%s\"\n", name);
        dprintk("  dir_name      = \"%s\"\n", dir_name);
        if (n_lfn_entries)
            dprintk("  lfn_name      = \"%s\"\n", lfn_dir_name);
        dprintk("  attributes    = 0x%02x\n", attributes);
        dprintk("  next_cluster  = %d\n", next_cluster);
        dprintk("  size          = %d B\n", le32(file_size));
        dprintk("\n");
    }

    return 0;
}

@private
fn fat32_build_fs_tree(pathname: uint8*, bs_info: struct fat32_bs_info*,
                       root_node: struct fs_node*, mount: struct fs_mount*) -> int32 {
    // build queue with all the clusters that start a chain
    let cluster_chains: queue<uint32>;
    queue_init(&cluster_chains, 10);
    defer queue_destroy(&cluster_chains);

    fat32_build_cluster_chains(bs_info, &cluster_chains);

    // create set of parent nodes
    let parent_nodes: set<uint32, struct fs_node*>;
    set_init(&parent_nodes, 10);
    defer set_destroy(&parent_nodes);

    // read clusters
    let n_chains = cluster_chains.length;
    let i: uint64 = 0;
    while (i < n_chains) {
        defer i = i + 1;

        // dprintk("[fat32] reading chain for cluster %d/%d\n", i, bs_info->n_fat_entries);
        let cluster = queue_pop(&cluster_chains);

        // retrieve parent node
        let parent_node: struct fs_node*;
        if (!set_get(&parent_nodes, cluster, &parent_node))
            parent_node = root_node;

        // ignore file contents
        if ((parent_node->attrs & FS_NODE_ATTRS_TYPE_MASK) == FS_NODE_ATTRS_TYPE_FILE)
            continue;

        // go through all clusters on the cluster chain
        while (true) {
            let status = fat32_read_cluster(pathname, bs_info, cluster, parent_node, &parent_nodes, mount);

            if (status < 0) return -1;
            if (status > 0) break;

            cluster = bs_info->fat_table[cluster] & 0x0FFFFFFF;

            if (!(cluster < FAT32_FAT_ENTRY_EOC and cluster < bs_info->n_fat_entries))
                break;
        }
    }

    return 0;
}

/**
 * Scans bs_info->fat_table and pushes the starting cluster number of each distinct chain into
 * cluster_chains. Iterates from root_cluster to n_fat_entries, marking visited clusters so each
 * chain is enqueued exactly once. FREE, RESERVED, and BAD entries are skipped.
 *
 * @param bs_info:        parsed boot sector info (fat_table and n_fat_entries must be set)
 * @param cluster_chains: output queue; must be initialized by the caller
 */
fn fat32_build_cluster_chains(bs_info: struct fat32_bs_info*, cluster_chains: struct queue<uint32>*) {
    let visited_clusters: bool* = alloc<bool>(bs_info->n_fat_entries as uint64);
    defer dealloc(visited_clusters);
    set_items(visited_clusters, false, bs_info->n_fat_entries as uint64);

    let i: uint32 = bs_info->root_cluster;
    while (i < bs_info->n_fat_entries) {
        defer i = i + 1;

        let cluster_value = bs_info->fat_table[i] & 0x0FFFFFFF;
        
        if (visited_clusters[i]) continue;
        visited_clusters[i] = true;

        if (cluster_value == FAT32_FAT_ENTRY_RESERVED or
            cluster_value == FAT32_FAT_ENTRY_FREE or
            cluster_value == FAT32_FAT_ENTRY_BAD)
            continue;

        queue_push(cluster_chains, i);

        let cluster = i;
        while(true) {
            // mark cluster as visited
            visited_clusters[cluster] = true;
            
            // next cluster
            cluster = bs_info->fat_table[cluster] & 0x0FFFFFFF;

            if(!(cluster < FAT32_FAT_ENTRY_EOC and cluster < bs_info->n_fat_entries))
                break;
        }
    }
}

/**
 * Mounts a FAT32 volume accessible at device_path. Reads the boot sector via vfs_read, validates
 * the FAT32 signature, parses the BPB, creates or resolves the VFS directory, registers a
 * mountpoint, reads the full FAT table, then recursively traverses all directories via
 * fat32_build_fs_tree.
 *
 * If mountpoint is NULL, the volume is mounted at "/volumes/<label>" (directory created
 * automatically). If mountpoint is non-NULL, the node at that path is resolved (following link
 * nodes to their target) and must be a folder.
 *
 * @param device_path: VFS path to the block device (e.g. "/dev/sda")
 * @param mountpoint:  VFS path to mount at, or NULL to auto-derive from the volume label
 *
 * @return 0 on success; or one of the FAT32_MOUNT_ERROR_* codes: -1 cannot read boot sector;
 *         -2 not a valid FAT32 volume; -3 mountpoint not found (vfs_create_dir or
 *         vfs_get_node_for_path returned null); -4 mountpoint is not a folder;
 *         -5 vfs_create_mountpoint failed; -6 FAT table read error; -7 fs tree build error
 */
fn fat32_mount(device_path: uint8*, mountpoint: uint8*) -> int64 {
    let status: int32;
    let bs: uint8[512];

    // read mbr
    if (vfs_read(device_path, bs, 512, 0) < 0) {
        dprintk("[fat32] cannot read boot sector from \"%s\"!\n", device_path);
        return FAT32_MOUNT_ERROR_CANNOT_READ_BOOT_SECTOR;
    }

    // check if it's a fat32 volume
    if (!fat32_is_boot_sector(bs)) {
        dprintk("[fat32] \"%s\" is not a FAT32 volume!\n", device_path);
        return FAT32_MOUNT_ERROR_VOLUME_IS_NOT_FAT32;
    }
    
    // parse boot sector
    dprintk("[fat32] parsing boot sector...\n");

    let bs_info = alloc<struct fat32_bs_info>(1);
    
    fat32_parse_boot_sector(bs, bs_info);

    dprintk("[fat32] found FAT32 volume \"%s\"!\n", bs_info->volume_label);
    dprintk("[fat32] first_fat_sector=%d, total_sectors=%d, table_size_32=%d, bs_info->total_clusters=%d\n",
            bs_info->first_fat_sector, bs_info->total_sectors, bs_info->table_size_32,
            bs_info->total_clusters);
    
    let root: struct fs_node*;
    let _mountpoint: uint8[50];
    if (mountpoint == null) {
        // create folder
        root = vfs_create_dir("/volumes", bs_info->volume_label, 0, null);
        if (root == null) {
            dprintk("[fat32] vfs_create_dir() returned NULL!\n");
            dealloc(bs_info);
            return FAT32_MOUNT_ERROR_MOUNTPOINT_NOT_FOUND;
        }

        // build volume name
        sprintf(_mountpoint, "/volumes/%s", bs_info->volume_label);

        mountpoint = _mountpoint;
    } else {
        // get folder
        root = vfs_get_node_for_path(mountpoint, null);

        if (root == null) {
            dprintk("[fat32] vfs_get_node_for_path() returned NULL!\n");
            dealloc(bs_info);
            return FAT32_MOUNT_ERROR_MOUNTPOINT_NOT_FOUND;
        }

        until ((root->attrs & FS_NODE_ATTRS_FLAG_LINK) == 0)
            root = root->child;
    }

    // check if root is a folder
    if ((root->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
        dprintk("[fat32] root is not a folder!\n");
        dealloc(bs_info);
        return FAT32_MOUNT_ERROR_MOUNTPOINT_IS_NOT_A_FOLDER;
    }
    
    // mount volume
    dprintk("[fat32] creating mountpoint \"%s\"...\n", mountpoint);

    let vfs_mp = vfs_create_mountpoint(mountpoint, device_path, bs_info, fat32_read, fat32_write);
    if (vfs_mp == null) {
        dprintk("[fat32] vfs_create_mountpoint() returned NULL!\n");
        
        dealloc(bs_info);
        fs_destroy_node(root);
        
        return FAT32_MOUNT_ERROR_CANNOT_CREATE_VFS_MOUNT;
    }

    // read fat table
    dprintk("[fat32] reading FAT table...\n");

    let fat_table_size = bs_info->table_size_32 * bs_info->n_bytes_per_sector;
    let fat_table = alloc<uint8>(fat_table_size as uint64);
    status = read_fat_table(device_path, bs_info, fat_table);
    if (status < 0) {
        dprintk("[fat32] read_fat_table() returned %d!\n", status);

        dealloc(fat_table);
        vfs_destroy_mountpoint(mountpoint);

        return FAT32_MOUNT_ERROR_CANNOT_READ_FAT_TABLE;
    }

    bs_info->fat_table = fat_table as uint32*;
    bs_info->n_fat_entries = fat_table_size / sizeof(uint32) as uint32;

    // build fs tree
    dprintk("[fat32] building fs tree\n");
    status = fat32_build_fs_tree(device_path, bs_info, root, vfs_mp);
    if (status < 0) {
        dprintk("[fat32] fat32_build_fs_tree() returned %i!\n", status);
        
        dealloc(fat_table);
        vfs_destroy_mountpoint(mountpoint);

        return FAT32_MOUNT_ERROR_CANNOT_BUILD_FS_TREE;
    }

    return 0;
}

@private
fn read_fat_table(pathname: uint8*, bs_info: struct fat32_bs_info*, fat_table: uint8*) -> int32 {
    let fat_table_addr: uint64 = bs_info->first_fat_sector as uint64 * bs_info->n_bytes_per_sector;
    dprintk("[fat32] will read %d sectors\n", bs_info->table_size_32);

    let i: uint64 = 0;
    while (i < bs_info->table_size_32) {
        let offset: uint64 = i * bs_info->n_bytes_per_sector;
        dprintk("[fat32] reading sector %d/%d, addr=%p\n",
                i + 1, bs_info->table_size_32, fat_table_addr + offset);

        if (vfs_read(pathname, &fat_table[offset], bs_info->n_bytes_per_sector as uint64,
                     fat_table_addr + offset) < 0)
            return -1;

        i = i + 1;
    }
    return bs_info->table_size_32 as int32;
}

/**
 * Unmounts the FAT32 volume mounted at the VFS path derived from pathname.
 * Frees bs_info->fat_table, then destroys the mountpoint via vfs_destroy_mountpoint.
 * Not yet implemented.
 *
 * @param device_path: VFS path to the block device used when mounting (e.g. "/dev/sda")
 *
 * @return 0 on success, -1 on error
 */
fn fat32_unmount(device_path: uint8*) -> int64 {
    // steps:
    //   1. get fs_mount
    //   2. get bs_info
    //   3. dealloc(bs_info->fat_table)
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
fn fat32_read(node: struct fs_node*, buffer: uint8*, count: uint64, offset: uint64) -> int64 {
    let entry_ref = node->info as struct fat32_entry_reference*;
    let fs_mp = node->mount;
    let bs_info = fs_mp->info as struct fat32_bs_info*;

    dprintk("[fat32] node: entry_ref=%p, fs_mp=%p, bs_info=%p\n", entry_ref, fs_mp,
            bs_info);
    dprintk("[fat32] fs_mp: mp=\"%s\", device=\"%s\"\n", fs_mp->mountpoint, fs_mp->device);
    dprintk("[fat32] entry_ref: cluster=%d, offset=%d, n_lfn_entries=%d\n", entry_ref->cluster,
            entry_ref->offset, entry_ref->n_lfn_entries);

    // read fat32_dir_entry
    let dir_entry_sector: uint32 = bs_info->first_data_sector + (entry_ref->cluster - bs_info->root_cluster);
    let dir_entry_size: uint32 = sizeof(struct fat32_dir_entry) as uint32;
    let dir_entry_offset: uint32 = dir_entry_sector * bs_info->n_bytes_per_sector +
                                (entry_ref->offset + entry_ref->n_lfn_entries) * dir_entry_size;
    let dir_entry = alloc<uint8>(dir_entry_size as uint64) as struct fat32_dir_entry*;
    defer dealloc(dir_entry);

    dprintk("[fat32] count=%d, offset=%p\n", dir_entry_size, dir_entry_offset);
    let status = vfs_read(fs_mp->device, dir_entry as uint8*, dir_entry_size as uint64, dir_entry_offset as uint64);
    if (status < 0) {
        dprintk("[fat32] vfs_read() returned %d!\n", status);
        return status;
    }

    // parse data_cluster and file_size
    let cluster_low: uint16;
    let cluster_high: uint16;
    let file_size: uint32;
    bytecopy(&cluster_low, &dir_entry->cluster_low, 1);
    bytecopy(&cluster_high, &dir_entry->cluster_high, 1);
    bytecopy(&file_size, &dir_entry->file_size, 1);

    let data_cluster: uint32 = (le16(cluster_high) as uint32 << 16) | le16(cluster_low);
    dprintk("[fat32] cluster=%d, file_size=%d\n", data_cluster, file_size);

    // validate offset
    if (file_size as uint64 <= offset) {
        dprintk("[fat32] offset must be less than the file size!\n");
        return -1;
    }

    // calculate which and how many sectors to read
    // total sectors used by the file contents
    let total_data_sectors: uint32 = file_size / bs_info->n_bytes_per_sector;
    // first data sector of the file contents to read
    let first_data_sector_to_read: uint64 = offset / bs_info->n_bytes_per_sector;
    // last data sector of the file contents to read
    let last_data_sector_to_read: uint64 = (offset + count - 1) / (bs_info->n_bytes_per_sector as uint64);
    let n_data_sectors_to_read: uint64 = last_data_sector_to_read - first_data_sector_to_read + 1;

    dprintk("[fat32] total_data_sectors=%d, first_data_sector_to_read=%d, last_data_sector_to_read=%d, n_data_sectors_to_read=%d\n",
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
    let tmp: uint8* = alloc<uint8>(n_data_sectors_to_read * bs_info->n_bytes_per_sector);
    defer dealloc(tmp);

    {
        let i: uint64 = 0;
        while (i < n_data_sectors_to_read) {
            // first data sector of the file contents
            let data_sector: uint32 = bs_info->first_data_sector + (cluster_to_read - bs_info->root_cluster);
            let data_offset: uint32 = data_sector * bs_info->n_bytes_per_sector;

            dprintk("[fat32] cluster_to_read=%d, data_sector=%d, data_offset=%p\n",
                    cluster_to_read, data_sector, data_offset);
            vfs_read(fs_mp->device, &tmp[i * bs_info->n_bytes_per_sector],
                    bs_info->n_bytes_per_sector as uint64, data_offset as uint64);

            // update cluster_to_read
            cluster_to_read = bs_info->fat_table[cluster_to_read];

            i = i + 1;
        }
    }

    // copy data to buffer, clamping to the bytes that actually remain in the file
    let bytes_read: uint64 = count;
    if (offset + count > file_size as uint64)
        bytes_read = file_size as uint64 - offset;

    bytecopy(buffer, &tmp[offset % bs_info->n_bytes_per_sector], bytes_read);

    return bytes_read as int64;
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
fn fat32_write(node: struct fs_node*, buffer: uint8*, count: uint64, offset: uint64) -> int64 {
    let entry_ref = node->info as struct fat32_entry_reference*;
    let fs_mp = node->mount;
    let bs_info = fs_mp->info as struct fat32_bs_info*;

    dprintk("[fat32] node: entry_ref=%p, fs_mp=%p, bs_info=%p\n", entry_ref, fs_mp,
            bs_info);
    dprintk("[fat32] fs_mp: mp=\"%s\", device=\"%s\"\n", fs_mp->mountpoint, fs_mp->device);
    dprintk("[fat32] entry_ref: cluster=%d, offset=%d, n_lfn_entries=%d\n", entry_ref->cluster,
            entry_ref->offset, entry_ref->n_lfn_entries);

    return -1;
}
