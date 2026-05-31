#include <debug.h>
#include <stdlib.h>
#include <string.h>
#include <arch/cpu.h>
#include "fat32.h"

void fat32_dump_boot_sector(struct mbr_boot_sector *boot_sector)
{
    printk("\r\n=== fat32 dump ===\r\n");

    // for (int i = 0; i < VIRTIO_MMIO_BLK_SECTOR_SIZE; i++)
    //     printk("%02x ", ((uint8_t *)&boot_sector)[i]);
    // printk("\r\n\r\n");

    // OEM name is not a null-terminated string
    char oem_name[9] = {0};
    strncpy(oem_name, boot_sector->oem_name, 8);

    // ARM doesn't like unaligned memory access
    struct _aligned_mbr_boot_sector data;
    memcpy(&data.mbr_jump_boot, &boot_sector->mbr_jump_boot, 3);
    memcpy(&data.n_bytes_per_sector, &boot_sector->n_bytes_per_sector, 2);
    memcpy(&data.n_sectors_per_cluster, &boot_sector->n_sectors_per_cluster, 1);
    memcpy(&data.n_reserved_sectors, &boot_sector->n_reserved_sectors, 2);
    memcpy(&data.n_fat, &boot_sector->n_fat, 1);
    memcpy(&data.n_root_dir_entries, &boot_sector->n_root_dir_entries, 1);
    memcpy(&data.total_sectors_16, &boot_sector->total_sectors_16, 2);
    memcpy(&data.media_dtor_type, &boot_sector->media_dtor_type, 1);
    memcpy(&data.n_sectors_per_fat, &boot_sector->n_sectors_per_fat, 2);
    memcpy(&data.n_sectors_per_track, &boot_sector->n_sectors_per_track, 2);
    memcpy(&data.n_heads, &boot_sector->n_heads, 2);
    memcpy(&data.n_hidden_sectors, &boot_sector->n_hidden_sectors, 2);
    memcpy(&data.total_sectors_32, &boot_sector->total_sectors_32, 4);
    memcpy(&data.mbr_signature_word, &boot_sector->mbr_signature_word, 4);

    printk("boot sector:\r\n");
    printk("jump_boot               = %02x %02x %02x\r\n", data.mbr_jump_boot[0], data.mbr_jump_boot[1], data.mbr_jump_boot[2]);
    printk("oem_name                = \"%s\"\r\n", oem_name);
    printk("n_bytes_per_sector      = %d\r\n", _le16(data.n_bytes_per_sector));
    printk("n_sectors_per_cluster   = %d\r\n", data.n_sectors_per_cluster);
    printk("n_reserved_sectors      = %d\r\n", _le16(data.n_reserved_sectors));
    printk("n_fat                   = %d\r\n", data.n_fat);
    printk("n_root_dir_entries      = %d\r\n", _le16(data.n_root_dir_entries));
    printk("total_sectors_16        = %d\r\n", _le16(data.total_sectors_16));
    printk("media_dtor_type         = %02x\r\n", data.media_dtor_type);
    printk("n_sectors_per_fat       = %d\r\n", _le16(data.n_sectors_per_fat));
    printk("n_sectors_per_track     = %d\r\n", _le16(data.n_sectors_per_track));
    printk("n_heads                 = %d\r\n", _le16(data.n_heads));
    printk("n_hidden_sectors        = %d\r\n", _le16(data.n_hidden_sectors));
    printk("total_sectors_32        = %d\r\n", _le32(data.total_sectors_32));
    printk("signature               = %04x\r\n", _le16(data.mbr_signature_word));

    if (data.total_sectors_32 > 0)
    {
        struct fat32_extended_boot_record *ext_br = (struct fat32_extended_boot_record *)&boot_sector->boot_code;

        // did I say already that ARM really doesn't like unaligned memory access?
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

        char volume_label[12] = {0};
        strncpy(volume_label, ext_br->volume_label, 11);

        char fat_type_label[9] = {0};
        strncpy(fat_type_label, ext_br->fat_type_label, 8);

        printk("\r\nextended boot record:\r\n");
        printk("table_size_32            = %d\r\n", _le32(ext_data.table_size_32));
        printk("extended_flags           = %04x\r\n", _le16(ext_data.extended_flags));
        printk("fat_version              = %d\r\n", _le16(ext_data.fat_version));
        printk("root_cluster             = %08x\r\n", _le32(ext_data.root_cluster));
        printk("fat_info                 = %04x\r\n", _le16(ext_data.fat_info));
        printk("backup_bs_sector         = %d\r\n", _le16(ext_data.backup_bs_sector));
        printk("drive_number             = %d\r\n", ext_data.drive_number);
        printk("boot_signature           = %02x\r\n", ext_data.boot_signature);
        printk("volume_id                = %08x\r\n", _le32(ext_data.volume_id));
        printk("volume_label             = \"%s\"\r\n", volume_label);
        printk("fat_type_label           = \"%s\"\r\n", fat_type_label);
    }

    printk("=================\r\n\r\n");
}