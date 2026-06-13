import "fs";

const VFS_IO_ERROR_FILE_NOT_FOUND = -1;
const VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND = -2;
const VFS_IO_ERROR_HANDLER_NOT_PROVIDED = -3;

/**
 * Initializes the VFS subsystem. Initializes the mount table hashmap, creates the global root tree
 * node, and adds a "volumes" subfolder to it. Must be called before any other VFS operation.
 */
@extern fn vfs_init();

/**
 * Registers a mount entry for the given path. Resolves mountpoint via _vfs_get_node, validates it
 * is a folder, then allocates a vfs_mount struct (storing device path and read/write handlers) and
 * inserts it into the mount table keyed by mountpoint. Does not insert any node into the VFS tree;
 * the caller is responsible for creating the node before mounting.
 *
 * @param mountpoint: null-terminated path of an existing VFS folder (e.g. "/volumes/NO NAME")
 * @param device:     VFS path of the underlying block device (e.g. "/dev/sd0"),
 *                    or NULL
 * @param info:       filesystem-private superblock data (heap-allocated; ownership transferred to
 *                    mount), or NULL
 * @param read:       read handler for this mount, or NULL
 * @param write:      write handler for this mount, or NULL
 *
 * @return pointer to the new vfs_mount on success, NULL if the mountpoint is not found or not a
 *         folder
 */
@extern
fn vfs_create_mountpoint(mountpoint: uint8*, device: uint8*, info: uint8*,
                         read: fn (struct fs_node*, uint8*, uint64, uint64) -> int32,
                         write: fn (struct fs_node*, uint8*, uint64, uint64) -> int32) -> struct fs_mount*;

/**
 * Looks up a mount entry by its exact mountpoint path.
 *
 * @param mountpoint: null-terminated mountpoint path (e.g. "/volumes/NO NAME")
 *
 * @return pointer to the matching vfs_mount, or NULL if not found
 */
@extern fn vfs_get_mountpoint(mountpoint: uint8*) -> struct fs_mount*;

/**
 * Resolves pathname via _vfs_get_node and returns the matching fs_node.
 *
 * @param pathname: null-terminated absolute path to resolve
 *
 * @return pointer to the fs_node, or NULL if not found
 */
@extern fn vfs_get_node_for_path(pathname: uint8*) -> struct fs_node*;

/**
 * Removes the mount entry for mountpoint from the mount table, unlinks the mount's root node from
 * its parent folder via fs_remove_child, destroys the root subtree via fs_destroy_node, and frees
 * the mountpoint, device, and info (if non-NULL) fields.
 *
 * @param mountpoint: null-terminated mountpoint path to unmount
 *
 * @return 0 on success, -1 if no matching mount entry is found
 */
@extern fn vfs_destroy_mountpoint(mountpoint: uint8*) -> int32;

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