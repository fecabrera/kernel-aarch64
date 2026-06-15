#pragma once

#include "filesystem.h"

#define FS_IO_ERROR_FILE_NOT_FOUND -1
#define FS_IO_ERROR_MOUNTPOINT_NOT_FOUND -2
#define FS_IO_ERROR_HANDLER_NOT_PROVIDED -3

typedef int (*vfs_handler_t)(struct fs_node *, uint8_t *, size_t, size_t);

struct vfs_mount {
    char *mountpoint; // VFS path this mount is registered under
    char *device;     // VFS path of the underlying block device, or NULL
    void *info;       // filesystem-private superblock data (heap-allocated; freed by
                      // vfs_destroy_mountpoint)
    struct fs_node *root;
    vfs_handler_t read;
    vfs_handler_t write;
};

/**
 * Initializes the VFS subsystem. Initializes the mount table hashmap, creates the global root tree
 * node, and adds a "volumes" subfolder to it. Must be called before any other VFS operation.
 */
void vfs_init();

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
struct vfs_mount *vfs_create_mountpoint(char *mountpoint, char *device, void *info,
                                        vfs_handler_t read, vfs_handler_t write);

/**
 * Looks up a mount entry by its exact mountpoint path.
 *
 * @param mountpoint: null-terminated mountpoint path (e.g. "/volumes/NO NAME")
 *
 * @return pointer to the matching vfs_mount, or NULL if not found
 */
struct vfs_mount *vfs_get_mountpoint(char *mountpoint);

/**
 * Resolves pathname via _vfs_get_node and returns the matching fs_node.
 *
 * @param pathname: null-terminated absolute path to resolve
 *
 * @return pointer to the fs_node, or NULL if not found
 */
struct fs_node *vfs_get_node_for_path(char *pathname, struct fs_node *root);

/**
 * Removes the mount entry for mountpoint from the mount table, unlinks the mount's root node from
 * its parent folder via fs_remove_child, destroys the root subtree via fs_destroy_node, and frees
 * the mountpoint, device, and info (if non-NULL) fields.
 *
 * @param mountpoint: null-terminated mountpoint path to unmount
 *
 * @return 0 on success, -1 if no matching mount entry is found
 */
int vfs_destroy_mountpoint(char *mountpoint);

/**
 * Resolves path via _vfs_get_node and creates a new subfolder named name inside it via
 * fs_add_subfolder.
 *
 * @param path:  null-terminated absolute path of the parent folder (e.g. "/volumes")
 * @param name:  null-terminated name for the new directory
 * @param attrs: attribute flags (FS_NODE_ATTRS_FLAG_*)
 * @param mount: vfs_mount pointer stored in node->mount (void * to avoid circular include), or NULL
 *
 * @return pointer to the new folder node, or NULL if the parent is not found or creation fails
 */
struct fs_node *vfs_create_dir(char *path, char *name, uint16_t attrs, void *mount);

/**
 * Resolves path via _vfs_get_node and creates a new file named name inside it via
 * fs_add_file_to_folder.
 *
 * @param path:      null-terminated absolute path of the parent folder (e.g. "/volumes/NO NAME")
 * @param name:      null-terminated name for the new file
 * @param file_size: file size in bytes, stored in node->file_size
 * @param attrs:     attribute flags (FS_NODE_ATTRS_FLAG_*)
 * @param mount:     vfs_mount pointer stored in node->mount (void * to avoid circular include), or
 *                   NULL
 *
 * @return pointer to the new file node, or NULL if the parent is not found or creation fails
 */
struct fs_node *vfs_create_file(char *path, char *name, size_t file_size, uint16_t attrs,
                                void *mount);

/**
 * Resolves pathname via _vfs_get_node and returns node->file_size.
 *
 * @param pathname: null-terminated absolute path of the file
 *
 * @return file size in bytes, or 0 if the node is not found
 */
size_t vfs_get_file_size(char *pathname);

/**
 * Resolves pathname via _vfs_get_node, reads node->mount for the covering mount, then dispatches to
 * mount->read.
 *
 * @param pathname: null-terminated absolute path of the file to read
 * @param buffer:   output buffer
 * @param count:    number of bytes to read
 * @param offset:   byte offset into the file to read from
 *
 * @return return value of mount->read on success; FS_IO_ERROR_FILE_NOT_FOUND,
 *         FS_IO_ERROR_MOUNTPOINT_NOT_FOUND, or FS_IO_ERROR_HANDLER_NOT_PROVIDED on failure
 */
int vfs_read(char *pathname, uint8_t *buffer, size_t count, size_t offset);

/**
 * Resolves pathname via _vfs_get_node, reads node->mount for the covering mount, then dispatches to
 * mount->write.
 *
 * @param pathname: null-terminated absolute path of the file to write
 * @param buffer:   input buffer
 * @param count:    number of bytes to write
 * @param offset:   byte offset into the file to write to
 *
 * @return return value of mount->write on success; FS_IO_ERROR_FILE_NOT_FOUND,
 *         FS_IO_ERROR_MOUNTPOINT_NOT_FOUND, or FS_IO_ERROR_HANDLER_NOT_PROVIDED on failure
 */
int vfs_write(char *pathname, uint8_t *buffer, size_t count, size_t offset);

/**
 * Prints the entire VFS tree to the kernel log via printk, starting from the global root's
 * children. Skips entering "." and ".." nodes to avoid cycles.
 */
void vfs_dump_fs();