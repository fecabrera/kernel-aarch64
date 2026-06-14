import "filesystem/fs";

struct io_module
{
    attrs: uint8;
    read: fn (uint8*, uint64, uint64, uint64) -> int32;
    write: fn (uint8*, uint64, uint64, uint64) -> int32;
    drv_info: uint64;
}

struct io_file
{
    pid: int64;
    module: struct io_module*;
}

/**
 * Initializes the I/O module registry. Creates the /dev folder in the VFS
 * tree and registers it as a mountpoint with io_read/io_write as handlers,
 * so vfs_read/vfs_write on /dev/<name> dispatch through the I/O registry.
 * Must be called after vfs_init() and before any other io_* function.
 */
@extern fn io_init();

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
@extern fn io_register_module(name: uint8*, drv_info: uint64,
                              read: fn (uint8*, uint64, uint64, uint64) -> int32,
                              write: fn (uint8*, uint64, uint64, uint64) -> int32) -> int32;

/**
 * Removes the named module from the registry, frees it, and unlinks the
 * corresponding /dev/<name> file node from the VFS tree.
 *
 * @param name: null-terminated module name to unregister
 *
 * @return 0 on success, -1 if the module is not found or the VFS node cannot be removed
 */
@extern fn io_unregister_module(name: uint8*) -> int32;

/**
 * Looks up the module keyed by node->name and calls its read handler.
 * Used as the vfs_handler_t read callback for the /dev mountpoint.
 *
 * @param node:   fs_node whose name identifies the target module
 * @param buff:   output buffer
 * @param count:  number of bytes to read
 * @param offset: byte offset into the device to read from
 *
 * @return return value of module->read, or -1 if the module is not found
 */
@extern fn io_read(node: struct fs_node*, buff: uint8*, count: uint64, offset: uint64) -> int32;

/**
 * Looks up the module keyed by node->name and calls its write handler.
 * Used as the vfs_handler_t write callback for the /dev mountpoint.
 *
 * @param node:   fs_node whose name identifies the target module
 * @param buff:   input buffer
 * @param count:  number of bytes to write
 * @param offset: byte offset into the device to write to
 *
 * @return return value of module->write, or -1 if the module is not found
 */
@extern fn io_write(node: struct fs_node*, buff: uint8*, count: uint64, offset: uint64) -> int32;