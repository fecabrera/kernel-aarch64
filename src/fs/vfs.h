#pragma once

/**
 * Initializes the VFS subsystem. Initializes the mount table hashmap, creates the global root tree
 * node, and adds a "volumes" subfolder to it. Must be called before any other VFS operation.
 */
void vfs_init();