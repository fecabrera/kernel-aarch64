#pragma once

#include "filesystem.h"

/**
 * Initializes the VFS subsystem. Creates the global root tree node and adds
 * a "volumes" subfolder to it. Must be called before vfs_mount, vfs_get_node,
 * or any other VFS operation.
 */
void vfs_init();

/**
 * Mounts a filesystem node at the given path in the VFS tree. Resolves the
 * mountpoint via vfs_get_node and appends node as a child of that folder.
 *
 * @param mountpoint: null-terminated path of an existing VFS folder (e.g. "/volumes")
 * @param node:       root fs_node of the filesystem to mount
 *
 * @return 0 on success, -1 if the mountpoint is not found or is not a folder
 */
int vfs_mount(char *mountpoint, struct fs_node *node);

/**
 * Unmounts the filesystem at the given path. Currently a stub; always returns 0.
 *
 * @param mountpoint: null-terminated mountpoint path to unmount
 *
 * @return 0
 */
int vfs_unmount(char *mountpoint);

/**
 * Resolves a slash-delimited absolute path in the VFS tree, starting from
 * the global root. Each path segment is matched against direct children via
 * fs_get_children, advancing one level per segment.
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