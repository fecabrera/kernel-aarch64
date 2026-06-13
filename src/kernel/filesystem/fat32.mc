import "fs";

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
@extern fn fat32_unmount(device_path: uint8*) -> int32;

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
@extern fn fat32_read(node: struct fs_node*, buffer: uint8*, count: uint64, offset: uint64) -> int32;

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
@extern fn fat32_write(node: struct fs_node*, buffer: uint8*, count: uint64, offset: uint64) -> int32;