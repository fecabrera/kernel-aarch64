#include <debug.h>
#include <uchar.h>
#include <stdlib.h>
#include <string.h>
#include <arch/cpu.h>
#include "fat32.h"

// ── Aligned copies ───────────────────────────────────────────────────────────
// Mirrors of the packed structs above without __attribute__((packed)).
// Use memcpy to transfer fields from a packed struct into one of these before
// accessing multi-byte fields, to avoid alignment faults on AArch64.
struct _aligned_fat32_extended_boot_record
{
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

struct _aligned_mbr_boot_sector
{
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

void fat32_dump_boot_sector(struct mbr_boot_sector *boot_sector)
{
    printk("\r\n=== fat32 dump ===\r\n");

    // for (int i = 0; i < VIRTIO_MMIO_BLK_SECTOR_SIZE; i++)
    //     printk("%02x ", ((uint8_t *)&boot_sector)[i]);
    // printk("\r\n\r\n");

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
    memcpy(&data.total_sectors_32, &boot_sector->total_sectors_32, 4);
    memcpy(&data.mbr_signature_word, &boot_sector->mbr_signature_word, 2);

    printk("boot sector:\r\n");
    printk("  jump_boot               = %02x %02x %02x\r\n", data.mbr_jump_boot[0], data.mbr_jump_boot[1], data.mbr_jump_boot[2]);
    printk("  oem_name                = ");
    for (int i = 0; i < 8; i++)
        printk("%02x ", boot_sector->oem_name[i]);
    printk("\r\n");
    printk("  n_bytes_per_sector      = %d\r\n", _le16(data.n_bytes_per_sector));
    printk("  n_sectors_per_cluster   = %d\r\n", data.n_sectors_per_cluster);
    printk("  n_reserved_sectors      = %d\r\n", _le16(data.n_reserved_sectors));
    printk("  n_fat                   = %d\r\n", data.n_fat);
    printk("  n_root_dir_entries      = %d\r\n", _le16(data.n_root_dir_entries));
    printk("  total_sectors_16        = %d\r\n", _le16(data.total_sectors_16));
    printk("  media_dtor_type         = %02x\r\n", data.media_dtor_type);
    printk("  table_size_16           = %d\r\n", _le16(data.table_size_16));
    printk("  n_sectors_per_track     = %d\r\n", _le16(data.n_sectors_per_track));
    printk("  n_heads                 = %d\r\n", _le16(data.n_heads));
    printk("  n_hidden_sectors        = %d\r\n", _le32(data.n_hidden_sectors));
    printk("  total_sectors_32        = %d\r\n", _le32(data.total_sectors_32));
    printk("  signature               = %04x\r\n", _le16(data.mbr_signature_word));

    if (data.total_sectors_32 > 0)
    {
        printk("\r\n");

        struct fat32_extended_boot_record *ext_br = (struct fat32_extended_boot_record *)&boot_sector->boot_code;
        fat32_dump_extended_boot_record(ext_br);
    }

    printk("=================\r\n\r\n");
}

void fat32_dump_extended_boot_record(struct fat32_extended_boot_record *ext_br)
{
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

    printk("extended boot record:\r\n");
    printk("  table_size_32           = %d\r\n", _le32(ext_data.table_size_32));
    printk("  extended_flags          = %04x\r\n", _le16(ext_data.extended_flags));
    printk("  fat_version             = %d\r\n", _le16(ext_data.fat_version));
    printk("  root_cluster            = %d\r\n", _le32(ext_data.root_cluster));
    printk("  fat_info                = %04x\r\n", _le16(ext_data.fat_info));
    printk("  backup_bs_sector        = %d\r\n", _le16(ext_data.backup_bs_sector));
    printk("  drive_number            = %d\r\n", ext_data.drive_number);
    printk("  boot_signature          = %02x\r\n", ext_data.boot_signature);
    printk("  volume_id               = %08x\r\n", _le32(ext_data.volume_id));
    printk("  volume_label            = ");
    for (int i = 0; i < 11; i++)
        printk("%02x ", ext_br->volume_label[i]);
    printk("\r\n");
    printk("  fat_type_label          = ");
    for (int i = 0; i < 8; i++)
        printk("%02x ", ext_br->fat_type_label[i]);
    printk("\r\n");
}

static void _fat32_dump_dir_entry(struct fat32_dir_entry *dir_entry)
{
    // thankfully the fat32_dir_entry is fairly aligned so we don't need to memcpy its data :)
    printk("dir entry:\r\n");
    printk("  name                    = ");
    for (int i = 0; i < 11; i++)
        printk("%02x ", dir_entry->name[i]);
    printk("\r\n");
    printk("  attributes              = %02x\r\n", dir_entry->attributes);
    printk("  reserved                = %02x\r\n", dir_entry->reserved);
    printk("  create_time_cs          = %02x\r\n", dir_entry->create_time_cs);
    printk("  create_time             = %d\r\n", _le16(dir_entry->create_time));
    printk("  create_date             = %d\r\n", _le16(dir_entry->create_date));
    printk("  access_date             = %d\r\n", _le16(dir_entry->access_date));
    printk("  cluster_high            = %04x\r\n", _le16(dir_entry->cluster_high));
    printk("  modify_time             = %d\r\n", _le16(dir_entry->modify_time));
    printk("  modify_date             = %d\r\n", _le16(dir_entry->modify_date));
    printk("  cluster_low             = %04x\r\n", _le16(dir_entry->cluster_low));
    printk("  file_size               = %d\r\n", _le32(dir_entry->file_size));
}

void fat32_dump_dir_entry(struct fat32_dir_entry *dir_entry)
{
    printk("\r\n=== fat32 dump ===\r\n");

    _fat32_dump_dir_entry(dir_entry);

    printk("=================\r\n\r\n");
}

static void _fat32_dump_lfn_entry(struct fat32_lfn_entry *lfn_dir_entry)
{
    // mostly aligned! except for the name* fields
    char16_t name1[5], name2[6], name3[2];

    memcpy((void *)name1, (void *)lfn_dir_entry->name1, 10);
    memcpy((void *)name2, (void *)lfn_dir_entry->name2, 12);
    memcpy((void *)name3, (void *)lfn_dir_entry->name3, 4);

    printk("lfn dir entry:\r\n");
    printk("  order                   = %02x\r\n", lfn_dir_entry->order);
    printk("  name1                   = ");
    for (int i = 0; i < 5; i++)
        printk("%04x ", _le16(name1[i]));
    printk("\r\n");
    printk("  attributes              = %02x\r\n", lfn_dir_entry->attributes);
    printk("  type                    = %02x\r\n", lfn_dir_entry->type);
    printk("  checksum                = %02x\r\n", lfn_dir_entry->checksum);
    printk("  name2                   = ");
    for (int i = 0; i < 6; i++)
        printk("%04x ", _le16(name2[i]));
    printk("\r\n");
    printk("  cluster                 = %d\r\n", _le16(lfn_dir_entry->cluster));
    printk("  name3                   = ");
    for (int i = 0; i < 2; i++)
        printk("%04x ", _le16(name3[i]));
    printk("\r\n\r\n");
}

void fat32_dump_lfn_entry(struct fat32_lfn_entry *lfn_dir_entry)
{
    printk("\r\n=== fat32 dump ===\r\n");

    _fat32_dump_lfn_entry(lfn_dir_entry);
    _fat32_dump_dir_entry((struct fat32_dir_entry *)(lfn_dir_entry + 1));

    printk("=================\r\n\r\n");
}