#pragma once

#include <stdint.h>
#include <uchar.h>

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
struct __attribute__((packed)) fat32_ext_bs
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
    uint16_t table_size_16;        // sectors per FAT (FAT12/16 only; 0 for FAT32)
    uint16_t n_sectors_per_track;  // sectors per track (CHS geometry)
    uint16_t n_heads;              // number of heads (CHS geometry)
    uint32_t n_hidden_sectors;     // hidden sectors preceding this partition
    uint32_t total_sectors_32;     // total sectors if >= 65536 (FAT32 always uses this)
    uint8_t boot_code[474];        // bootstrap code + EBR (for FAT32, EBR starts at offset 36)
    uint16_t mbr_signature_word;   // boot sector signature (must be 0xAA55)
};

// ── Directory entry attributes ────────────────────────────────────────────────

#define FAT32_ATTR_READ_ONLY (1 << 0) // file is read-only
#define FAT32_ATTR_HIDDEN (1 << 1)    // file is hidden
#define FAT32_ATTR_SYSTEM (1 << 2)    // file is a system file
#define FAT32_ATTR_VOLUME_ID (1 << 3) // entry is a volume label
#define FAT32_ATTR_DIRECTORY (1 << 4) // entry is a subdirectory
#define FAT32_ATTR_ARCHIVE (1 << 5)   // file has been modified since last backup
// combination used to mark a long file name entry
#define FAT32_ATTR_LFN (FAT32_ATTR_READ_ONLY | FAT32_ATTR_HIDDEN | FAT32_ATTR_SYSTEM | FAT32_ATTR_VOLUME_ID)

// ── Special first-byte values for dir_entry.name[0] ──────────────────────────

#define FAT32_DIRENT_FREE 0xE5 // entry was deleted (slot is free)
#define FAT32_DIRENT_END 0x00  // no more entries in this directory
#define FAT32_DIRENT_DOT 0x2E  // "." or ".." entry

// ── FAT entry values ──────────────────────────────────────────────────────────

#define FAT32_FAT_ENTRY_FREE 0x00000000     // cluster is free
#define FAT32_FAT_ENTRY_RESERVED 0x00000001 // reserved, do not use
#define FAT32_FAT_ENTRY_BAD 0x0FFFFFF7      // cluster is bad
#define FAT32_FAT_ENTRY_EOC 0x0FFFFFF8      // end of chain (any value >= this is EOC)

// ── LFN sequence number flags ─────────────────────────────────────────────────

#define FAT32_LFN_LAST_ENTRY 0x40 // OR'd into lfn_entry.order for the last (highest) LFN entry

/**
 * FAT32 8.3 short-name directory entry (32 bytes).
 * All multi-byte fields are little-endian.
 * Use memcpy when accessing uint16_t/uint32_t fields through a pointer.
 */
struct __attribute__((packed)) fat32_dir_entry
{
    uint8_t name[11];       // short name: 8 chars name + 3 chars extension, space-padded
    uint8_t attributes;     // FAT32_ATTR_* flags
    uint8_t reserved;       // reserved for Windows NT
    uint8_t create_time_cs; // creation time, 10ms units (0–199)
    uint16_t create_time;   // creation time: bits[15:11]=hour, [10:5]=minute, [4:0]=second/2
    uint16_t create_date;   // creation date: bits[15:9]=year-1980, [8:5]=month, [4:0]=day
    uint16_t access_date;   // last access date (same format as create_date)
    uint16_t cluster_high;  // high 16 bits of first cluster (FAT32 only)
    uint16_t modify_time;   // last modified time (same format as create_time)
    uint16_t modify_date;   // last modified date (same format as create_date)
    uint16_t cluster_low;   // low 16 bits of first cluster
    uint32_t file_size;     // file size in bytes (0 for directories)
};

/**
 * FAT32 long file name (LFN) directory entry (32 bytes).
 * LFN entries precede their corresponding 8.3 entry in reverse order.
 * Name characters are UTF-16LE; unused trailing chars are 0xFFFF.
 */
struct __attribute__((packed)) fat32_lfn_entry
{
    uint8_t order;      // sequence number (1-based); OR'd with FAT32_LFN_LAST_ENTRY if last
    char16_t name1[5];  // UTF-16LE name characters 1–5
    uint8_t attributes; // always FAT32_ATTR_LFN (0x0F)
    uint8_t type;       // always 0
    uint8_t checksum;   // checksum of the 8.3 short name
    char16_t name2[6];  // UTF-16LE name characters 6–11
    uint16_t cluster;   // always 0x0000
    char16_t name3[2];  // UTF-16LE name characters 12–13
};

// struct fat32_node
// {
//     size_t entry_cluster;
//     size_t entry_index;
//     size_t data_cluster;
//     size_t size;
//     char name[256];
// };

struct fat32_bs_info
{
    // fields from MBR boot sector
    char volume_label[12]; // volume label from EBR, null-terminated
    uint8_t n_fat;
    uint8_t n_sectors_per_cluster;
    uint16_t n_reserved_sectors;
    uint16_t n_bytes_per_sector;
    uint32_t total_sectors_32;
    // fields from FAT32 extended boot record
    uint8_t drive_number;
    uint32_t root_cluster;
    uint32_t table_size_32;
    // derived fields
    uint32_t first_fat_sector;
    uint32_t first_data_sector;
    uint32_t data_sectors;
    uint32_t total_clusters;
};

typedef uint32_t fat_table_entry_t; // bitfield: see FAT_TABLE_ENTRY_TYPE_* and FAT_TABLE_ENTRY_MASK_*

/**
 * Prints all BPB fields of the MBR boot sector to the kernel log via printk.
 * Reads multi-byte fields with memcpy to avoid alignment faults.
 *
 * @param boot_sector: pointer to a packed MBR boot sector read from disk
 */
void fat32_dump_boot_sector(struct mbr_boot_sector *boot_sector);

/**
 * Prints all fields of a FAT32 extended boot record to the kernel log.
 * Reads multi-byte fields with memcpy to avoid alignment faults.
 *
 * @param ext_br: pointer to the EBR, typically at &boot_sector->boot_code[0]
 */
void fat32_dump_extended_boot_record(struct fat32_ext_bs *ext_br);

/**
 * Prints all fields of an 8.3 directory entry to the kernel log.
 * Reads multi-byte fields with memcpy to avoid alignment faults.
 *
 * @param dir_entry: pointer to a packed fat32_dir_entry
 */
void fat32_dump_dir_entry(struct fat32_dir_entry *dir_entry);

/**
 * Prints all fields of a long file name directory entry to the kernel log.
 *
 * @param lfn_dir_entry: pointer to a packed fat32_lfn_entry
 */
void fat32_dump_lfn_entry(struct fat32_lfn_entry *lfn_dir_entry);

/**
 * Checks whether a 512-byte sector buffer contains a valid FAT32 boot sector.
 * Verifies the boot signature (0xAA55), and that the EBR fields table_size_32
 * and root_cluster are non-zero.
 *
 * @param buff: 512-byte sector buffer to inspect
 *
 * @return non-zero if the buffer looks like a valid FAT32 boot sector, zero otherwise
 */
int fat32_is_boot_sector(uint8_t *buff);

/**
 * Parses a raw 512-byte boot sector buffer into a fat32_bs_info struct.
 * Reads BPB fields from mbr_boot_sector and EBR fields from the embedded
 * fat32_ext_bs using memcpy to avoid alignment faults. Also computes derived
 * fields: first_fat_sector, first_data_sector, data_sectors, total_clusters.
 * Copies the EBR volume label into bs_info->volume_label, null-terminated
 * and with trailing spaces stripped.
 *
 * @param buff:    512-byte buffer containing the boot sector read from disk
 * @param bs_info: output struct populated with parsed and derived fields
 */
void fat32_parse_boot_sector(uint8_t *buff, struct fat32_bs_info *bs_info);

/**
 * Copies raw FAT cluster chain values from a single pre-read sector buffer
 * into fat_table, applying _le32 and masking to 28 bits per the spec.
 * Appends entries starting at fat_table[sector_offset * n_entries_per_sector].
 * Each entry holds the raw next-cluster value; compare against FAT32_FAT_ENTRY_*
 * to interpret (free, bad, EOC, or next cluster number).
 * Call once per FAT sector, in order, to build the full table.
 *
 * @param bs_info:       parsed boot sector info (used for sector/entry sizes)
 * @param buff:          512-byte buffer containing the FAT sector already read from disk
 * @param sector_offset: index of this sector within the FAT (0-based)
 * @param fat_table:     output array; must hold at least bs_info->table_size_32 entries
 *
 * @return number of entries written into fat_table
 */
uint32_t fat32_read_fat_table(struct fat32_bs_info *bs_info, uint8_t *buff, uint32_t sector_offset, fat_table_entry_t *fat_table);