#pragma once

#include "filesystem.h"

#define VFS_IO_ERROR_FILE_NOT_FOUND -1
#define VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND -2
#define VFS_IO_ERROR_HANDLER_NOT_PROVIDED -3

typedef int (*vfs_handler_t)(struct fs_node *, uint8_t *, size_t, size_t);

struct vfs_mount
{
    char *mountpoint; // VFS path this mount is registered under
    char *device;     // VFS path of the underlying block device, or NULL
    struct fs_node *root;
    vfs_handler_t read;
    vfs_handler_t write;
};

/**
 * Initializes the VFS subsystem. Initializes the mount table hashmap, creates
 * the global root tree node, and adds a "volumes" subfolder to it. Must be
 * called before any other VFS operation.
 */
void vfs_init();

/**
 * Registers a mount entry for the given path. Resolves mountpoint via
 * _vfs_get_node, validates it is a folder, then allocates a vfs_mount
 * struct (storing device path and read/write handlers) and inserts it into the
 * mount table keyed by mountpoint. Does not insert any node into the VFS
 * tree; the caller is responsible for creating the node before mounting.
 *
 * @param mountpoint: null-terminated path of an existing VFS folder (e.g. "/volumes/NO NAME")
 * @param device:     VFS path of the underlying block device (e.g. "/dev/sd0"), or NULL
 * @param read:       read handler for this mount, or NULL
 * @param write:      write handler for this mount, or NULL
 *
 * @return 0 on success, -1 if the mountpoint is not found, -2 if it is not a folder
 */
int vfs_create_mountpoint(char *mountpoint, char *device, vfs_handler_t read, vfs_handler_t write);

/**
 * Looks up a mount entry by its exact mountpoint path.
 *
 * @param mountpoint: null-terminated mountpoint path (e.g. "/volumes/NO NAME")
 *
 * @return pointer to the matching vfs_mount, or NULL if not found
 */
struct vfs_mount *vfs_get_mountpoint(char *mountpoint);

/**
 * Walks pathname left-to-right, accumulating each segment and checking it
 * against the mount table at every '/' boundary. Returns the last mount whose
 * path is a prefix of pathname, or NULL if no mount covers any prefix.
 *
 * @param pathname: null-terminated absolute path to resolve (e.g. "/volumes/NO NAME/foo")
 *
 * @return pointer to the deepest matching vfs_mount, or NULL if none found
 */
struct vfs_mount *vfs_get_mountpoint_for_path(char *pathname);

/**
 * Removes the mount entry for mountpoint from the mount table, unlinks the
 * mount's root node from its parent folder via fs_remove_child, destroys
 * the root subtree via fs_destroy_node, and frees the mountpoint and device
 * strings.
 *
 * @param mountpoint: null-terminated mountpoint path to unmount
 *
 * @return 0 on success, -1 if no matching mount entry is found
 */
int vfs_destroy_mountpoint(char *mountpoint);

/**
 * Resolves path via _vfs_get_node and creates a new subfolder named name
 * inside it via fs_add_subfolder.
 *
 * @param path:  null-terminated absolute path of the parent folder (e.g. "/volumes")
 * @param name:  null-terminated name for the new directory
 * @param attrs: attribute flags (FS_NODE_ATTRS_FLAG_*)
 *
 * @return pointer to the new folder node, or NULL if the parent is not found or creation fails
 */
struct fs_node *vfs_create_dir(char *path, char *name, uint16_t attrs);

/**
 * Resolves path via _vfs_get_node and creates a new file named name
 * inside it via fs_add_file_to_folder.
 *
 * @param path:  null-terminated absolute path of the parent folder (e.g. "/volumes/NO NAME")
 * @param name:  null-terminated name for the new file
 * @param attrs: attribute flags (FS_NODE_ATTRS_FLAG_*)
 *
 * @return pointer to the new file node, or NULL if the parent is not found or creation fails
 */
struct fs_node *vfs_create_file(char *path, char *name, uint16_t attrs);

/**
 * Resolves pathname via _vfs_get_node and finds its covering mount via
 * vfs_get_mountpoint_for_path, then dispatches to mount->read.
 *
 * @param pathname: null-terminated absolute path of the file to read
 * @param buffer:   output buffer
 * @param count:    number of bytes to read
 * @param offset:   byte offset into the file to read from
 *
 * @return return value of mount->read on success;
 *         VFS_IO_ERROR_FILE_NOT_FOUND, VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND, or
 *         VFS_IO_ERROR_HANDLER_NOT_PROVIDED on failure
 */
int vfs_read(char *pathname, uint8_t *buffer, size_t count, size_t offset);

/**
 * Resolves pathname via _vfs_get_node and finds its covering mount via
 * vfs_get_mountpoint_for_path, then dispatches to mount->write.
 *
 * @param pathname: null-terminated absolute path of the file to write
 * @param buffer:   input buffer
 * @param count:    number of bytes to write
 * @param offset:   byte offset into the file to write to
 *
 * @return return value of mount->write on success;
 *         VFS_IO_ERROR_FILE_NOT_FOUND, VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND, or
 *         VFS_IO_ERROR_HANDLER_NOT_PROVIDED on failure
 */
int vfs_write(char *pathname, uint8_t *buffer, size_t count, size_t offset);

/**
 * Prints the entire VFS tree to the kernel log via printk, starting from the
 * global root's children. Skips entering "." and ".." nodes to avoid cycles.
 */
void vfs_dump_fs();