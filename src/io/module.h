#pragma once

#include <string.h>
#include <sched/process.h>

#define IO_CAN_READ (1 << 0)
#define IO_CAN_WRITE (1 << 1)

typedef int (*io_handler_t)(uint8_t *, size_t);

struct io_module
{
    uint8_t attrs;
    io_handler_t read;
    io_handler_t write;
};

struct io_file
{
    pid_t pid;
    struct io_module *module;
};

/**
 * Initializes the I/O module registry and creates the /dev folder in the VFS
 * tree. Must be called after vfs_init() and before any other io_* function.
 */
void io_init();

/**
 * Allocates and registers a named I/O module. The module is stored in an
 * internal hashmap64 keyed by name. Also creates a file node at /dev/<name>
 * in the VFS tree.
 *
 * @param name:  null-terminated module name (e.g. "tty0")
 * @param attrs: capability flags (IO_CAN_READ, IO_CAN_WRITE)
 * @param read:  read handler, or NULL if not readable
 * @param write: write handler, or NULL if not writable
 *
 * @return 0 on success, -1 if a module with that name is already registered
 */
int io_register_module(char *name, uint8_t attrs, io_handler_t read, io_handler_t write);

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
 * Looks up the named module and calls its read handler.
 *
 * @param name:   null-terminated module name
 * @param buffer: output buffer
 * @param n:      number of bytes to read
 *
 * @return return value of module->read, or -1 if the module is not found
 */
int io_read(char *name, uint8_t *buffer, size_t n);

/**
 * Looks up the named module and calls its write handler.
 *
 * @param name:   null-terminated module name
 * @param buffer: input buffer
 * @param n:      number of bytes to write
 *
 * @return return value of module->write, or -1 if the module is not found
 */
int io_write(char *name, uint8_t *buffer, size_t n);