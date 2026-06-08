#pragma once

#include <string.h>
#include <fs/filesystem.h>
#include <sched/process.h>

typedef int (*io_handler_t)(uint8_t *, size_t, uint64_t);

struct io_module
{
    uint8_t attrs;
    io_handler_t read;
    io_handler_t write;
    uint64_t drv_info;
};

struct io_file
{
    pid_t pid;
    struct io_module *module;
};

/**
 * Initializes the I/O module registry. Creates the /dev folder in the VFS
 * tree and registers it as a mountpoint with io_read/io_write as handlers,
 * so vfs_read/vfs_write on /dev/<name> dispatch through the I/O registry.
 * Must be called after vfs_init() and before any other io_* function.
 */
void io_init();

/**
 * Allocates and registers a named I/O module. The module is stored in an
 * internal hashmap64 keyed by name. Also creates a file node at /dev/<name>
 * in the VFS tree.
 *
 * @param name:     null-terminated module name (e.g. "tty0")
 * @param drv_info: driver-private value passed as the third argument to read/write handlers
 * @param read:     read handler, or NULL if not readable
 * @param write:    write handler, or NULL if not writable
 *
 * @return 0 on success, -1 if a module with that name is already registered
 */
int io_register_module(char *name, uint64_t drv_info, io_handler_t read, io_handler_t write);

/**
 * Removes the named module from the registry, frees it, and unlinks the
 * corresponding /dev/<name> file node from the VFS tree.
 *
 * @param name: null-terminated module name to unregister
 *
 * @return 0 on success, -1 if the module is not found or the VFS node cannot be removed
 */
int io_unregister_module(char *name);

/**
 * Looks up the module keyed by node->name and calls its read handler.
 * Used as the vfs_handler_t read callback for the /dev mountpoint.
 *
 * @param node:   fs_node whose name identifies the target module
 * @param buff: output buffer
 * @param n:      number of bytes to read
 *
 * @return return value of module->read, or -1 if the module is not found
 */
int io_read(struct fs_node *node, uint8_t *buff, size_t n);

/**
 * Looks up the module keyed by node->name and calls its write handler.
 * Used as the vfs_handler_t write callback for the /dev mountpoint.
 *
 * @param node:   fs_node whose name identifies the target module
 * @param buff: input buffer
 * @param n:      number of bytes to write
 *
 * @return return value of module->write, or -1 if the module is not found
 */
int io_write(struct fs_node *node, uint8_t *buff, size_t n);