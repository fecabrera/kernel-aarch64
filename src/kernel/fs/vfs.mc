/**
 * Resolves pathname via _vfs_get_node and returns node->file_size.
 *
 * @param pathname: null-terminated absolute path of the file
 *
 * @return file size in bytes, or 0 if the node is not found
 */
@extern fn vfs_get_file_size(pathname: uint8*) -> uint64;

/**
 * Resolves pathname via _vfs_get_node, reads node->mount for the covering mount, then dispatches to
 * mount->read.
 *
 * @param pathname: null-terminated absolute path of the file to read
 * @param buffer:   output buffer
 * @param count:    number of bytes to read
 * @param offset:   byte offset into the file to read from
 *
 * @return return value of mount->read on success; VFS_IO_ERROR_FILE_NOT_FOUND,
 *         VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND, or VFS_IO_ERROR_HANDLER_NOT_PROVIDED on failure
 */
@extern fn vfs_read(pathname: uint8*, buffer: uint8*, count: uint64, offset: uint64) -> int32;

/**
 * Resolves pathname via _vfs_get_node, reads node->mount for the covering mount, then dispatches to
 * mount->write.
 *
 * @param pathname: null-terminated absolute path of the file to write
 * @param buffer:   input buffer
 * @param count:    number of bytes to write
 * @param offset:   byte offset into the file to write to
 *
 * @return return value of mount->write on success; VFS_IO_ERROR_FILE_NOT_FOUND,
 *         VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND, or VFS_IO_ERROR_HANDLER_NOT_PROVIDED on failure
 */
@extern fn vfs_write(pathname: uint8*, buffer: uint8*, count: uint64, offset: uint64) -> int32;

/**
 * Prints the entire VFS tree to the kernel log via printk, starting from the global root's
 * children. Skips entering "." and ".." nodes to avoid cycles.
 */
@extern fn vfs_dump_fs();