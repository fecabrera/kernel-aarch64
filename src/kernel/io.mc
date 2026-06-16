import "debug";
import "cpu";
import "dict";
import "filesystem/fs";
import "filesystem/vfs";

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

@static let _devices: struct dict<struct io_module*>;
@static let _dev_root: struct fs_node* = null;
@static let dev_mp: struct fs_mount* = null;

/**
 * Initializes the I/O module registry. Creates the /dev folder in the VFS
 * tree and registers it as a mountpoint with io_read/io_write as handlers,
 * so vfs_read/vfs_write on /dev/<name> dispatch through the I/O registry.
 * Must be called after vfs_init() and before any other io_* function.
 */
fn io_init() {
    _dev_root = vfs_create_dir("/", "dev", 0, null);
    if (_dev_root == null) {
        dprintk("[virtio_mmio@%x] cannot creat mountpoint \"%s\"!\r\n");
        hang();
    }
    dev_mp = vfs_create_mountpoint("/dev", null, null, io_read, io_write);

    dict_init(&_devices, 10);
}

/**
 * Looks up a registered module by name in the internal registry.
 *
 * @param name: null-terminated module name (e.g. "serial")
 *
 * @return pointer to the matching io_module, or null if no module is registered
 *         under that name
 */
@private
fn io_get_module(name: uint8*) -> struct io_module* {
    let mod: struct io_module*;
    if (!dict_get(&_devices, name, &mod))
        return null;

    return mod;
}

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
fn io_register_module(name: uint8*, drv_info: uint64,
                      read: fn (uint8*, uint64, uint64, uint64) -> int32,
                      write: fn (uint8*, uint64, uint64, uint64) -> int32) -> int32 {
    if (io_get_module(name) != null) {
        dprintk("[io] there's already a \"%s\" module!\r\n", name);
        return -1;
    }

    let mod: struct io_module* = alloc<struct io_module>(1);
    mod->drv_info = drv_info;
    mod->read = read;
    mod->write = write;

    if (fs_add_file_to_folder(_dev_root, name, 0, 0, null, dev_mp) == null) {
        dprintk("[io] cannot add \"%s\" to /dev!\r\n", name);
        return -1;
    }

    dict_set(&_devices, name, mod);

    return 0;
}

/**
 * Removes the named module from the registry, frees it, and unlinks the
 * corresponding /dev/<name> file node from the VFS tree.
 *
 * @param name: null-terminated module name to unregister
 *
 * @return 0 on success, -1 if the module is not found or the VFS node cannot be removed
 */
fn io_unregister_module(name: uint8*) -> int32 {
    let mod = io_get_module(name);
    if (mod == null) {
        dprintk("[io] module \"%s\" not found!\r\n", name);
        return -1;
    }

    if (fs_remove_child(_dev_root, name) < 0) {
        dprintk("[io] cannot remove \"%s\" from /dev!\r\n", name);
        return -1;
    }

    dict_remove(&_devices, name);

    return 0;
}


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
fn io_read(node: struct fs_node*, buff: uint8*, count: uint64, offset: uint64) -> int32 {
    dprintk("[io] read(): mod=\"%s\", buff=0x%08X, count=%d, offset=%d\r\n",
            node->name, buff, count, offset);

    let module = io_get_module(node->name);
    if (module == null) {
        dprintk("[io] module \"%s\" not found!\r\n", node->name);
        return -1;
    }

    return module->read(buff, count, offset, module->drv_info);
}

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
fn io_write(node: struct fs_node*, buff: uint8*, count: uint64, offset: uint64) -> int32 {
    dprintk("[io] write(): mod=\"%s\", buff=0x%08X, count=%d, offset=%d\r\n",
            node->name, buff, count, offset);

    let module = io_get_module(node->name);
    if (module == null) {
        dprintk("[io] module \"%s\" not found!\r\n", node->name);
        return -1;
    }

    return module->write(buff, count, offset, module->drv_info);
}