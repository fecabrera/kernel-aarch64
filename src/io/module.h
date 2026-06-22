#pragma once

/**
 * Initializes the I/O module registry. Creates the /dev folder in the VFS
 * tree and registers it as a mountpoint with io_read/io_write as handlers,
 * so vfs_read/vfs_write on /dev/<name> dispatch through the I/O registry.
 * Must be called after vfs_init() and before any other io_* function.
 */
void io_init();