#include "fat32.h"
#include "fs/filesystem.h"
#include <arch/cpu.h>
#include <debug.h>
#include <dsa/set.h>
#include <dsa/stack.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <uchar.h>

// ── Aligned copies ───────────────────────────────────────────────────────────
// Mirrors of the packed structs above without __attribute__((packed)).
// Use memcpy to transfer fields from a packed struct into one of these before
// accessing multi-byte fields, to avoid alignment faults on AArch64.
struct _aligned_fat32_extended_boot_record {
    uint32_t table_size_32;
    uint16_t extended_flags;
    uint16_t fat_version;
    uint32_t root_cluster;
    uint16_t fat_info;
    uint16_t backup_bs_sector;
    uint8_t drive_number;
    uint8_t reserved_1;
    uint8_t boot_signature;
    uint32_t volume_id;
};

struct _aligned_mbr_boot_sector {
    uint8_t mbr_jump_boot[3];
    uint16_t n_bytes_per_sector;
    uint8_t n_sectors_per_cluster;
    uint16_t n_reserved_sectors;
    uint8_t n_fat;
    uint16_t n_root_dir_entries;
    uint16_t total_sectors_16;
    uint8_t media_dtor_type;
    uint16_t table_size_16;
    uint16_t n_sectors_per_track;
    uint16_t n_heads;
    uint32_t n_hidden_sectors;
    uint32_t total_sectors_32;
    uint16_t mbr_signature_word;
};

void fat32_dump_boot_sector(struct mbr_boot_sector *boot_sector) {
    dprintk("\n=== fat32 dump ===\n");

    // ARM doesn't like unaligned memory access
    struct _aligned_mbr_boot_sector data;
    memcpy(&data.mbr_jump_boot, &boot_sector->mbr_jump_boot, 3);
    memcpy(&data.n_bytes_per_sector, &boot_sector->n_bytes_per_sector, 2);
    memcpy(&data.n_sectors_per_cluster, &boot_sector->n_sectors_per_cluster, 1);
    memcpy(&data.n_reserved_sectors, &boot_sector->n_reserved_sectors, 2);
    memcpy(&data.n_fat, &boot_sector->n_fat, 1);
    memcpy(&data.n_root_dir_entries, &boot_sector->n_root_dir_entries, 2);
    memcpy(&data.total_sectors_16, &boot_sector->total_sectors_16, 2);
    memcpy(&data.media_dtor_type, &boot_sector->media_dtor_type, 1);
    memcpy(&data.table_size_16, &boot_sector->table_size_16, 2);
    memcpy(&data.n_sectors_per_track, &boot_sector->n_sectors_per_track, 2);
    memcpy(&data.n_heads, &boot_sector->n_heads, 2);
    memcpy(&data.n_hidden_sectors, &boot_sector->n_hidden_sectors, 4);
    memcpy(&data.total_sectors_16, &boot_sector->total_sectors_16, 4);
    memcpy(&data.total_sectors_32, &boot_sector->total_sectors_32, 4);
    memcpy(&data.mbr_signature_word, &boot_sector->mbr_signature_word, 2);

    dprintk("boot sector:\n");
    dprintk("  jump_boot               = %02x %02x %02x\n", data.mbr_jump_boot[0],
            data.mbr_jump_boot[1], data.mbr_jump_boot[2]);
    dprintk("  oem_name                = ");
    for (int i = 0; i < 8; i++)
        dprintk("%02x ", boot_sector->oem_name[i]);
    dprintk("\n");
    dprintk("  n_bytes_per_sector      = %d\n", _le16(data.n_bytes_per_sector));
    dprintk("  n_sectors_per_cluster   = %d\n", data.n_sectors_per_cluster);
    dprintk("  n_reserved_sectors      = %d\n", _le16(data.n_reserved_sectors));
    dprintk("  n_fat                   = %d\n", data.n_fat);
    dprintk("  n_root_dir_entries      = %d\n", _le16(data.n_root_dir_entries));
    dprintk("  total_sectors_16        = %d\n", _le16(data.total_sectors_16));
    dprintk("  media_dtor_type         = %02x\n", data.media_dtor_type);
    dprintk("  table_size_16           = %d\n", _le16(data.table_size_16));
    dprintk("  n_sectors_per_track     = %d\n", _le16(data.n_sectors_per_track));
    dprintk("  n_heads                 = %d\n", _le16(data.n_heads));
    dprintk("  n_hidden_sectors        = %d\n", _le32(data.n_hidden_sectors));
    dprintk("  total_sectors_16        = %d\n", _le16(data.total_sectors_16));
    dprintk("  total_sectors_32        = %d\n", _le32(data.total_sectors_32));
    dprintk("  signature               = %04x\n", _le16(data.mbr_signature_word));

    if (data.total_sectors_32 > 0) {
        dprintk("\n");

        struct fat32_ext_bs *ext_br = (struct fat32_ext_bs *)&boot_sector->boot_code;
        fat32_dump_extended_boot_record(ext_br);
    }

    dprintk("=================\n\n");
}

void fat32_dump_extended_boot_record(struct fat32_ext_bs *ext_br) {
    // did I mentioned that ARM really doesn't like unaligned memory access?
    struct _aligned_fat32_extended_boot_record ext_data;
    memcpy(&ext_data.table_size_32, &ext_br->table_size_32, 4);
    memcpy(&ext_data.volume_id, &ext_br->volume_id, 4);
    memcpy(&ext_data.extended_flags, &ext_br->extended_flags, 2);
    memcpy(&ext_data.fat_version, &ext_br->fat_version, 2);
    memcpy(&ext_data.root_cluster, &ext_br->root_cluster, 4);
    memcpy(&ext_data.fat_info, &ext_br->fat_info, 2);
    memcpy(&ext_data.backup_bs_sector, &ext_br->backup_bs_sector, 2);
    memcpy(&ext_data.drive_number, &ext_br->drive_number, 1);
    memcpy(&ext_data.boot_signature, &ext_br->boot_signature, 2);

    dprintk("extended boot record:\n");
    dprintk("  table_size_32           = %d\n", _le32(ext_data.table_size_32));
    dprintk("  extended_flags          = %04x\n", _le16(ext_data.extended_flags));
    dprintk("  fat_version             = %d\n", _le16(ext_data.fat_version));
    dprintk("  root_cluster            = %d\n", _le32(ext_data.root_cluster));
    dprintk("  fat_info                = %04x\n", _le16(ext_data.fat_info));
    dprintk("  backup_bs_sector        = %d\n", _le16(ext_data.backup_bs_sector));
    dprintk("  drive_number            = %d\n", ext_data.drive_number);
    dprintk("  boot_signature          = %02x\n", ext_data.boot_signature);
    dprintk("  volume_id               = %08x\n", _le32(ext_data.volume_id));
    dprintk("  volume_label            = ");
    for (int i = 0; i < 11; i++)
        dprintk("%02x ", ext_br->volume_label[i]);
    dprintk("\n");
    dprintk("  fat_type_label          = ");
    for (int i = 0; i < 8; i++)
        dprintk("%02x ", ext_br->fat_type_label[i]);
    dprintk("\n");
}

static void _fat32_dump_dir_entry(struct fat32_dir_entry *dir_entry) {
    // thankfully the fat32_dir_entry is fairly aligned so we don't need to memcpy its data :)
    dprintk("dir entry:\n");
    dprintk("  name                    = ");
    for (int i = 0; i < 11; i++)
        dprintk("%02x ", dir_entry->name[i]);
    dprintk("\n");
    dprintk("  attributes              = %02x\n", dir_entry->attributes);
    dprintk("  reserved                = %02x\n", dir_entry->reserved);
    dprintk("  create_time_cs          = %02x\n", dir_entry->create_time_cs);
    dprintk("  create_time             = %d\n", _le16(dir_entry->create_time));
    dprintk("  create_date             = %d\n", _le16(dir_entry->create_date));
    dprintk("  access_date             = %d\n", _le16(dir_entry->access_date));
    dprintk("  cluster_high            = %04x\n", _le16(dir_entry->cluster_high));
    dprintk("  modify_time             = %d\n", _le16(dir_entry->modify_time));
    dprintk("  modify_date             = %d\n", _le16(dir_entry->modify_date));
    dprintk("  cluster_low             = %04x\n", _le16(dir_entry->cluster_low));
    dprintk("  file_size               = %d\n", _le32(dir_entry->file_size));
}

void fat32_dump_dir_entry(struct fat32_dir_entry *dir_entry) {
    dprintk("\n=== fat32 dump ===\n");

    _fat32_dump_dir_entry(dir_entry);

    dprintk("=================\n\n");
}

static void _fat32_dump_lfn_entry(struct fat32_lfn_entry *lfn_dir_entry) {
    // mostly aligned! except for the name* fields
    char16_t name1[5], name2[6], name3[2];

    memcpy((void *)name1, (void *)lfn_dir_entry->name1, 10);
    memcpy((void *)name2, (void *)lfn_dir_entry->name2, 12);
    memcpy((void *)name3, (void *)lfn_dir_entry->name3, 4);

    dprintk("lfn dir entry:\n");
    dprintk("  order                   = %02x\n", lfn_dir_entry->order);
    dprintk("  name1                   = ");
    for (int i = 0; i < 5; i++)
        dprintk("%04x ", _le16(name1[i]));
    dprintk("\n");
    dprintk("  attributes              = %02x\n", lfn_dir_entry->attributes);
    dprintk("  type                    = %02x\n", lfn_dir_entry->type);
    dprintk("  checksum                = %02x\n", lfn_dir_entry->checksum);
    dprintk("  name2                   = ");
    for (int i = 0; i < 6; i++)
        dprintk("%04x ", _le16(name2[i]));
    dprintk("\n");
    dprintk("  cluster                 = %d\n", _le16(lfn_dir_entry->cluster));
    dprintk("  name3                   = ");
    for (int i = 0; i < 2; i++)
        dprintk("%04x ", _le16(name3[i]));
    dprintk("\n\n");
}

void fat32_dump_lfn_entry(struct fat32_lfn_entry *lfn_dir_entry) {
    dprintk("\n=== fat32 dump ===\n");

    _fat32_dump_lfn_entry(lfn_dir_entry);
    _fat32_dump_dir_entry((struct fat32_dir_entry *)(lfn_dir_entry + 1));

    dprintk("=================\n\n");
}

int fat32_is_boot_sector(uint8_t *buff) {
    struct mbr_boot_sector *bs = (struct mbr_boot_sector *)buff;

    // check boot sector signature
    uint16_t signature;
    memcpy(&signature, &bs->mbr_signature_word, 2);
    if (_le16(signature) != 0xAA55)
        return 0;

    // check FAT32-specific fields in the extended boot record
    struct fat32_ext_bs *ext_br = (struct fat32_ext_bs *)bs->boot_code;
    if (ext_br->table_size_32 == 0 || ext_br->root_cluster == 0)
        return 0;

    return 1;
}

void fat32_parse_boot_sector(uint8_t *buff, struct fat32_bs_info *bs_info) {
    struct mbr_boot_sector *bs = (struct mbr_boot_sector *)buff;
    struct fat32_ext_bs *ext_br = (struct fat32_ext_bs *)bs->boot_code;

    // extract volume label from EBR, which is more likely to be human-friendly than the OEM name in
    // the MBR boot sector. Also trim trailing spaces.
    strntrimend(bs_info->volume_label, (char *)ext_br->volume_label, 11);

    // ARM doesn't like unaligned memory access, so we need to memcpy the fields into local
    // variables before using them
    uint8_t n_fat, n_sectors_per_cluster;
    uint16_t n_reserved_sectors, n_bytes_per_sector, total_sectors_16;
    uint32_t total_sectors_32;
    memcpy(&n_sectors_per_cluster, &bs->n_sectors_per_cluster, 1);
    memcpy(&n_fat, &bs->n_fat, 1);
    memcpy(&n_reserved_sectors, &bs->n_reserved_sectors, 2);
    memcpy(&n_bytes_per_sector, &bs->n_bytes_per_sector, 2);
    memcpy(&total_sectors_16, &bs->total_sectors_16, 2);
    memcpy(&total_sectors_32, &bs->total_sectors_32, 4);

    // extended boot record fields
    uint8_t drive_number;
    uint32_t root_cluster, table_size_32;
    memcpy(&drive_number, &ext_br->drive_number, 1);
    memcpy(&root_cluster, &ext_br->root_cluster, 4);
    memcpy(&table_size_32, &ext_br->table_size_32, 4);

    // derived fields
    uint32_t total_sectors = total_sectors_16 == 0 ? total_sectors_32 : total_sectors_16;
    uint32_t first_fat_sector = n_reserved_sectors;
    uint32_t first_data_sector = n_reserved_sectors + (table_size_32 * n_fat);
    uint32_t data_sectors = total_sectors - first_data_sector;
    uint32_t total_clusters = data_sectors / n_sectors_per_cluster;

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

uint32_t fat32_read_fat_table(struct fat32_bs_info *bs_info, uint8_t *buff,
                              uint32_t sector_offset) {
    uint16_t n_entries_per_sector = bs_info->n_bytes_per_sector / 4;
    uint32_t offset = sector_offset * n_entries_per_sector;

    dprintk("fat sector %d (0x%08x):\n", sector_offset,
            (bs_info->first_fat_sector + sector_offset) * n_entries_per_sector);

    fat_table_entry_t *_fat_table = bs_info->fat_table + offset;
    uint32_t *_entry = (uint32_t *)buff;

    uint32_t i;
    for (i = 0; i < n_entries_per_sector && offset + i < bs_info->table_size_32; i++) {
        uint32_t entry_value = _le32(_entry[i]) & 0x0FFFFFFF;
        if (entry_value)
            dprintk("  cluster %d: 0x%08x\n", offset + i, entry_value);
        _fat_table[i] = entry_value;
    }

    return i;
}

static int _fat32_read_cluster(char *pathname, struct fat32_bs_info *bs_info, uint32_t cluster,
                               struct fs_node *parent_node, struct set64 *parent_nodes,
                               struct vfs_mount *mount) {
    uint32_t sector_offset = cluster - bs_info->root_cluster;
    uint32_t sector = bs_info->first_data_sector + sector_offset;

    dprintk("cluster %d (0x%08x):\n", cluster, sector * bs_info->n_bytes_per_sector);
    uint16_t n_entries_per_sector = bs_info->n_bytes_per_sector / 32;

    // allocate buffer
    uint8_t *buff = (uint8_t *)kmalloc(bs_info->n_bytes_per_sector);

    int status =
        vfs_read(pathname, buff, bs_info->n_bytes_per_sector, sector * bs_info->n_bytes_per_sector);
    if (status < 0) {
        dprintk("[fat32] vfs_read() returned %i!\n", status);
        kfree(buff);
        return -1;
    }

    struct fat32_dir_entry *first_entry = (struct fat32_dir_entry *)buff;
    for (uint32_t offset = 0; offset < n_entries_per_sector; offset++) {
        struct fat32_dir_entry *dir_entry = first_entry + offset;

        if ((dir_entry->name[0] == FAT32_DIRENT_END)) {
            kfree(buff);
            return 1;
        }

        if ((dir_entry->name[0] == FAT32_DIRENT_FREE) ||
            (dir_entry->name[0] == FAT32_ATTR_SYSTEM) ||
            (dir_entry->name[0] == FAT32_ATTR_VOLUME_ID))
            continue;

        dprintk("entry #%d: \n", offset);

        struct stack64 lfn_entries;
        stack64_init(&lfn_entries, 10);

        // copy offset
        uint32_t _offset = offset;

        // parse lfn entries
        uint8_t attributes;
        int n_lfn_entries = 0;
        while (true) {
            memcpy(&attributes, &dir_entry->attributes, 1);

            if (attributes != FAT32_ATTR_LFN)
                break;

            offset++;
            n_lfn_entries++;

            struct fat32_lfn_entry *lfn_entry = (struct fat32_lfn_entry *)dir_entry;
            stack64_push(&lfn_entries, (uintptr_t)lfn_entry);

            dir_entry = (struct fat32_dir_entry *)dir_entry + 1;
        }

        memcpy(&attributes, &dir_entry->attributes, 1);

        char dir_name[12] = {0};
        char *lfn_dir_name = (char *)kmalloc(n_lfn_entries * 13 + 1);

        strncpy(dir_name, (char *)dir_entry->name, 12);

        // parse lfn name
        for (int i = 0; i < n_lfn_entries; i++) {
            struct fat32_lfn_entry *lfn_entry = (struct fat32_lfn_entry *)stack64_pop(&lfn_entries);

            char16_t _lfn_dir_name[26] = {0};
            memcpy(_lfn_dir_name, lfn_entry->name1, 10);
            memcpy(_lfn_dir_name + 5, lfn_entry->name2, 12);
            memcpy(_lfn_dir_name + 11, lfn_entry->name3, 4);

            utf16lencpy(lfn_dir_name + (i * 13), (char16_t *)_lfn_dir_name, 26);
        }

        uint16_t cluster_low, cluster_high;
        uint32_t file_size;
        memcpy(&cluster_low, &dir_entry->cluster_low, 2);
        memcpy(&cluster_high, &dir_entry->cluster_high, 2);
        memcpy(&file_size, &dir_entry->file_size, 4);

        uint32_t next_cluster = ((uint32_t)_le16(cluster_high) << 16) | _le16(cluster_low);

        // get actual name
        char *name;
        if (n_lfn_entries) {
            size_t len = strlen(lfn_dir_name);
            name = (char *)kmalloc(len + 1);
            strncpy(name, lfn_dir_name, len + 1);
        } else {
            name = (char *)kmalloc(13);
            size_t name_len = strntrimend(name, dir_name, 8);
            if (strntrimend(name + name_len + 1, dir_name + 8, 3) > 0)
                name[name_len] = '.';
            else
                name[name_len] = '\0';
        }

        if (strncmp(name, ".", 2) && strncmp(name, "..", 3)) {
            struct fat32_entry_reference *entry_ref =
                (struct fat32_entry_reference *)kmalloc(sizeof(struct fat32_entry_reference));
            entry_ref->cluster = cluster;
            entry_ref->offset = _offset;
            entry_ref->n_lfn_entries = n_lfn_entries;

            struct fs_node *node;
            if (attributes & FAT32_ATTR_DIRECTORY)
                node = fs_add_subfolder(parent_node, name, 0, (void *)entry_ref, (void *)mount);
            else {
                uint32_t file_size;
                memcpy(&file_size, &dir_entry->file_size, sizeof(uint32_t));
                node = fs_add_file_to_folder(parent_node, name, file_size, 0, (void *)entry_ref,
                                             (void *)mount);
            }

            set64_set(parent_nodes, next_cluster, (uintptr_t)node);
        }

        dprintk("  name          = \"%s\"\n", name);
        dprintk("  dir_name      = \"%s\"\n", dir_name);
        if (n_lfn_entries)
            dprintk("  lfn_name      = \"%s\"\n", lfn_dir_name);
        dprintk("  attributes    = 0x%02x\n", attributes);
        dprintk("  next_cluster  = %d\n", next_cluster);
        dprintk("  size          = %d B\n", _le32(file_size));
        dprintk("\n");

        kfree(lfn_dir_name);
        kfree(name);
        stack64_destroy(&lfn_entries);
    }

    kfree(buff);
    return 0;
}

void fat32_build_cluster_chains(struct fat32_bs_info *bs_info, struct queue32 *cluster_chains) {
    bool *visited_clusters = (bool *)kmalloc(bs_info->n_fat_entries);
    memset(visited_clusters, 0, bs_info->n_fat_entries);

    for (uint32_t i = bs_info->root_cluster; i < bs_info->n_fat_entries; i++) {
        uint32_t cluster_value = bs_info->fat_table[i] & 0x0FFFFFFF;

        // skip if cluster has been visited
        if (visited_clusters[i])
            continue;

        visited_clusters[i] = true;

        // skip if cluster is reserved, free or bad
        if (cluster_value == FAT32_FAT_ENTRY_RESERVED || cluster_value == FAT32_FAT_ENTRY_FREE ||
            cluster_value == FAT32_FAT_ENTRY_BAD)
            continue;

        queue32_push(cluster_chains, i);

        uint32_t cluster = i;
        do {
            // mark cluster as visited
            visited_clusters[cluster] = true;

            // next cluster
            cluster = bs_info->fat_table[cluster] & 0x0FFFFFFF;
        } while (cluster < FAT32_FAT_ENTRY_EOC && cluster < bs_info->n_fat_entries);
    }

    kfree(visited_clusters);
}

static int _fat32_build_fs_tree(char *pathname, struct fat32_bs_info *bs_info,
                                struct fs_node *root_node, struct vfs_mount *mount) {
    // build queue with all the clusters that start a chain
    struct queue32 cluster_chains;
    queue32_init(&cluster_chains, 10);

    fat32_build_cluster_chains(bs_info, &cluster_chains);

    // create set of parent nodes
    struct set64 parent_nodes;
    set64_init(&parent_nodes, 10);

    // read clusters
    uint32_t n_chains = cluster_chains.length;
    for (uint32_t i = 0; i < n_chains; i++) {
        // dprintk("[fat32] reading chain for cluster %d/%d\n", i, bs_info->n_fat_entries);
        uint32_t cluster = queue32_pop(&cluster_chains);

        // retrieve parent node
        uint64_t parent_node_ptr;
        struct fs_node *parent_node;
        if (set64_get(&parent_nodes, cluster, &parent_node_ptr) == 0)
            parent_node = root_node;
        else
            parent_node = (struct fs_node *)parent_node_ptr;

        // ignore file contents
        if ((parent_node->attrs & FS_NODE_ATTRS_TYPE_MASK) == FS_NODE_ATTRS_TYPE_FILE)
            continue;

        // go through all clusters on the cluster chain
        do {
            // dprintk("[fat32] reading cluster %d\n", cluster);

            int status =
                _fat32_read_cluster(pathname, bs_info, cluster, parent_node, &parent_nodes, mount);
            if (status < 0) {
                queue32_destroy(&cluster_chains);
                set64_destroy(&parent_nodes);
                return -1;
            }
            if (status > 0)
                break;

            cluster = bs_info->fat_table[cluster] & 0x0FFFFFFF;
        } while (cluster < FAT32_FAT_ENTRY_EOC && cluster < bs_info->n_fat_entries);
    }

    queue32_destroy(&cluster_chains);
    set64_destroy(&parent_nodes);
    return 0;
}

static int _read_fat_table(char *pathname, struct fat32_bs_info *bs_info, uint8_t *fat_table) {
    size_t fat_table_addr = bs_info->first_fat_sector * bs_info->n_bytes_per_sector;
    dprintk("[fat32] will read %d sectors\n", bs_info->table_size_32);

    for (size_t i = 0; i < bs_info->table_size_32; i++) {
        size_t offset = i * bs_info->n_bytes_per_sector;
        dprintk("\e[A");
        dprintk("[fat32] reading sector %d/%d, addr=0x%08X\n", i + 1, bs_info->table_size_32,
                fat_table_addr + offset);

        if (vfs_read(pathname, fat_table + offset, bs_info->n_bytes_per_sector,
                     fat_table_addr + offset) < 0)
            return -1;
    }
    return bs_info->table_size_32;
}

int fat32_mount(char *device_path, char *mountpoint) {
    int status;
    uint8_t bs[512];
    char _mountpoint[50] = {0};

    // read mbr
    if (vfs_read(device_path, bs, 512, 0) < 0) {
        dprintk("[fat32] cannot read boot sector from \"%s\"!\n", device_path);
        return -1;
    }

    // check if it's a fat32 volume
    if (!fat32_is_boot_sector(bs)) {
        dprintk("[fat32] \"%s\" is not a FAT32 volume!\n", device_path);
        return -2;
    }

    // parse boot sector
    dprintk("[fat32] parsing boot sector...\n");
    struct fat32_bs_info *bs_info = (struct fat32_bs_info *)kmalloc(sizeof(struct fat32_bs_info));
    fat32_parse_boot_sector(bs, bs_info);

    dprintk("[fat32] found FAT32 volume \"%s\"!\n", bs_info->volume_label);
    dprintk("[fat32] first_fat_sector=%d, total_sectors=%d, table_size_32=%d, "
            "bs_info->total_clusters=%d\n",
            bs_info->first_fat_sector, bs_info->total_sectors, bs_info->table_size_32,
            bs_info->total_clusters);

    struct fs_node *root;

    if (mountpoint == NULL) {
        // create folder
        root = vfs_create_dir("/volumes", bs_info->volume_label, 0, NULL);
        if (root == NULL) {
            dprintk("[fat32] vfs_create_dir() returned NULL!\n");
            kfree(bs_info);
            return -3;
        }

        // build volume name
        sprintf(_mountpoint, "/volumes/%s", bs_info->volume_label);

        mountpoint = _mountpoint;
    } else {
        root = vfs_get_node_for_path(mountpoint, NULL);
        if (root == NULL) {
            dprintk("[fat32] vfs_get_node_for_path() returned NULL!\n");
            kfree(bs_info);
            return -4;
        }
    }

    // mount volume
    dprintk("[fat32] creating mountpoint \"%s\"...\n", mountpoint);

    struct vfs_mount *vfs_mp =
        vfs_create_mountpoint(mountpoint, device_path, bs_info, &fat32_read, &fat32_write);
    if (vfs_mp == NULL) {
        dprintk("[fat32] vfs_create_mountpoint() returned NULL!\n");

        fs_destroy_node(root);
        kfree(bs_info);

        return -5;
    }

    // read fat table
    dprintk("[fat32] reading FAT table...\n");

    size_t fat_table_size = bs_info->table_size_32 * bs_info->n_bytes_per_sector;
    fat_table_entry_t *fat_table = (fat_table_entry_t *)kmalloc(fat_table_size);
    status = _read_fat_table(device_path, bs_info, (uint8_t *)fat_table);
    if (status < 0) {
        dprintk("[fat32] _read_fat_table() returned %d!\n", status);

        kfree(fat_table);
        vfs_destroy_mountpoint(mountpoint);

        return -6;
    }

    bs_info->fat_table = (fat_table_entry_t *)fat_table;
    bs_info->n_fat_entries = fat_table_size / sizeof(fat_table_entry_t);

    // build fs tree
    dprintk("[fat32] building fs tree\n");
    status = _fat32_build_fs_tree(device_path, bs_info, root, vfs_mp);
    if (status < 0) {
        dprintk("[fat32] _fat32_build_fs_tree() returned %i!\n", status);

        kfree(fat_table);
        vfs_destroy_mountpoint(mountpoint);

        return -7;
    }

    return 0;
}
