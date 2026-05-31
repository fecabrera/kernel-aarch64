#pragma once

#include <stdint.h>

// Partition type byte
#define MBR_PARTITION_TYPE_EMPTY 0
#define MBR_PARTITION_TYPE_NTFS 0x07
#define MBR_PARTITION_TYPE_FAT32_CHS 0x0B
#define MBR_PARTITION_TYPE_FAT32_LBA 0x0C
#define MBR_PARTITION_TYPE_EXTENDED 0x0F
#define MBR_PARTITION_TYPE_LINUX_SWAP 0x82
#define MBR_PARTITION_TYPE_LINUX_FILESYSTEM 0x83

// Media descriptor byte — stored in BPB_Media and FAT[0] low byte
#define MBR_MEDIA_DESCRIPTOR_REMOVABLE 0xF0 // 3.5" / 5.25" removable disk
#define MBR_MEDIA_DESCRIPTOR_FIXED 0xF8     // fixed (non-removable) disk
#define MBR_MEDIA_DESCRIPTOR_F0_ALT 0xF9    // double-sided, 80 tracks, 15 sectors
#define MBR_MEDIA_DESCRIPTOR_FC 0xFC        // single-sided, 40 tracks, 9 sectors
#define MBR_MEDIA_DESCRIPTOR_FD 0xFD        // double-sided, 40 tracks, 9 sectors
#define MBR_MEDIA_DESCRIPTOR_FE 0xFE        // single-sided, 40 tracks, 8 sectors
#define MBR_MEDIA_DESCRIPTOR_FF 0xFF        // double-sided, 40 tracks, 8 sectors

/**
 * FAT32 extended boot record (EBR), occupies bytes 36–61 of the
 * FAT32 volume boot sector, immediately after the common BPB fields.
 * All multi-byte fields are little-endian.
 */
struct __attribute__((packed)) fat32_extended_boot_record
{
    uint32_t table_size_32;    // sectors per FAT (FAT32 only)
    uint16_t extended_flags;   // mirroring flags; bit 7: single active FAT, bits 3:0: active FAT number
    uint16_t fat_version;      // FAT32 version (high byte = major, low = minor; typically 0x0000)
    uint32_t root_cluster;     // cluster number of the root directory (usually 2)
    uint16_t fat_info;         // sector number of the FSInfo structure
    uint16_t backup_bs_sector; // sector number of the backup boot sector (usually 6)
    uint8_t reserved_0[12];
    uint8_t drive_number; // BIOS drive number (0x00 = floppy, 0x80 = hard disk)
    uint8_t reserved_1;
    uint8_t boot_signature;    // extended boot signature (0x29 = volume_id/label/type fields present)
    uint32_t volume_id;        // volume serial number
    uint8_t volume_label[11];  // volume label, padded with spaces (not null-terminated)
    uint8_t fat_type_label[8]; // always "FAT32   " (informational only, not for type detection)
};

/**
 * Master Boot Record boot sector (sector 0 of the disk).
 * Contains the BIOS Parameter Block (BPB), bootstrap code, and the
 * 512-byte boot sector signature. All multi-byte fields are little-endian.
 * Use memcpy to read uint16_t/uint32_t fields to avoid alignment faults.
 */
struct __attribute__((packed)) mbr_boot_sector
{
    uint8_t mbr_jump_boot[3];      // x86 jump instruction to bootstrap code
    uint8_t oem_name[8];           // OEM name string, NOT null-terminated (e.g. "mkfs.fat")
    uint16_t n_bytes_per_sector;   // bytes per sector (typically 512)
    uint8_t n_sectors_per_cluster; // sectors per allocation cluster (power of 2)
    uint16_t n_reserved_sectors;   // reserved sectors before first FAT (includes this sector)
    uint8_t n_fat;                 // number of FAT copies (usually 2)
    uint16_t n_root_dir_entries;   // root directory entries (0 for FAT32)
    uint16_t total_sectors_16;     // total sectors if < 65536, else 0 (use total_sectors_32)
    uint8_t media_dtor_type;       // media descriptor byte (see MBR_MEDIA_DESCRIPTOR_*)
    uint16_t n_sectors_per_fat;    // sectors per FAT (FAT12/16 only; 0 for FAT32)
    uint16_t n_sectors_per_track;  // sectors per track (CHS geometry)
    uint16_t n_heads;              // number of heads (CHS geometry)
    uint16_t n_hidden_sectors;     // hidden sectors preceding this partition
    uint32_t total_sectors_32;     // total sectors if >= 65536 (FAT32 always uses this)
    uint8_t boot_code[476];        // bootstrap code + EBR (for FAT32, EBR starts at offset 36)
    uint16_t mbr_signature_word;   // boot sector signature (must be 0xAA55)
};

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
    uint16_t n_sectors_per_fat;
    uint16_t n_sectors_per_track;
    uint16_t n_heads;
    uint16_t n_hidden_sectors;
    uint32_t total_sectors_32;
    uint16_t mbr_signature_word;
};

/**
 * Prints all fields of the MBR boot sector to the kernel log via printk.
 * Reads multi-byte fields with memcpy to avoid alignment faults.
 *
 * @param boot_sector: pointer to a packed MBR boot sector read from disk
 */
void fat32_dump_boot_sector(struct mbr_boot_sector *boot_sector);