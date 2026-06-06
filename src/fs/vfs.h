#pragma once

#include <io/module.h>
#include "filesystem.h"

struct vfs_mount
{
    char *mountpoint;
    struct fs_node *root;
    struct io_module *module;
    void *data;
};

/**
 * Initializes the VFS subsystem. Creates the global root tree node and adds
 * a "volumes" subfolder to it. Must be called before vfs_mount, vfs_get_node,
 * or any other VFS operation.
 */
void vfs_init();

/**
 * Registers a mount entry for the given path. Resolves mountpoint via
 * vfs_get_node, validates it is a folder, then allocates a vfs_mount
 * struct (storing module and data) and pushes it onto the mount table.
 * Does not insert any node into the VFS tree; the caller is responsible
 * for creating the node before mounting.
 *
 * @param mountpoint: null-terminated path of an existing VFS folder (e.g. "/volumes/NO NAME")
 * @param module:     io_module backing this mount, or NULL
 * @param data:       driver-private context (e.g. virtio slot cast to void *), or NULL
 *
 * @return 0 on success, -1 if the mountpoint is not found, -2 if it is not a folder
 */
int vfs_create_mountpoint(char *mountpoint, struct io_module *module, void *data);

/**
 * Removes the mount entry for mountpoint from the mount table, unlinks the
 * mount's root node from its parent folder via fs_remove_child, and destroys
 * the root subtree via fs_destroy_node.
 *
 * @param mountpoint: null-terminated mountpoint path to unmount
 *
 * @return 0 on success, -1 if no matching mount entry is found
 */
int vfs_destroy_mountpoint(char *mountpoint);

/**
 * Resolves a slash-delimited absolute path in the VFS tree, starting from
 * the global root. Each path segment is matched against direct children via
 * fs_get_child, advancing one level per segment.
 *
 * @param pathname: null-terminated absolute path (e.g. "/volumes")
 *
 * @return pointer to the matching fs_node, or NULL if any segment is not found
 */
struct fs_node *vfs_get_node(char *pathname);

/**
 * Prints the entire VFS tree to the kernel log via printk, starting from the
 * global root's children. Skips entering "." and ".." nodes to avoid cycles.
 */
void vfs_dump_fs();