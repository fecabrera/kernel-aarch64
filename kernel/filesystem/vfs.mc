import "debug";
import "fs";
import "mm";
import "list";
import "dict";

@static let _vfs_root: struct fs_node*;
@static let _vfs_mp_table: struct dict<struct fs_mount*>;

/**
 * Initializes the VFS subsystem. Initializes the mount table hashmap, creates the global root tree
 * node, and adds a "volumes" subfolder to it. Must be called before any other VFS operation.
 */
fn vfs_init() {
    // initialize table
    dict_init(&_vfs_mp_table, 10);

    // create root and volumes
    _vfs_root = fs_create_folder(null, node_attrs::READ, null, null);
    fs_add_subfolder(_vfs_root, "volumes", node_attrs::READ, null, null);
}

/**
 * Returns the global VFS root node. Valid only after vfs_init().
 *
 * @return the root fs_node of the VFS tree
 */
fn vfs_root() -> struct fs_node* {
    return _vfs_root;
}

/**
 * Registers a mount entry for the given path. Resolves mountpoint via vfs_get_node_for_path, validates it
 * is a folder, then allocates a fs_mount struct (storing device path and read/write handlers) and
 * inserts it into the mount table keyed by mountpoint. Does not insert any node into the VFS tree;
 * the caller is responsible for creating the node before mounting.
 *
 * @param mountpoint: null-terminated path of an existing VFS folder (e.g. "/volumes/NO NAME")
 * @param device:     VFS path of the underlying block device (e.g. "/dev/sd0"),
 *                    or null
 * @param info:       filesystem-private superblock data (heap-allocated; ownership transferred to
 *                    mount), or null
 * @param read:       read handler for this mount, or null
 * @param write:      write handler for this mount, or null
 *
 * @return pointer to the new fs_mount on success, null if the mountpoint is not found or not a
 *         folder
 */
fn vfs_create_mountpoint(mountpoint: uint8*, device: uint8*, info: uint8*,
                         read: fn (struct fs_node*, uint8*, uint64, uint64) -> int64,
                         write: fn (struct fs_node*, uint8*, uint64, uint64) -> int64) -> struct fs_mount* {
    let mp_node = vfs_get_node_for_path(mountpoint, null);
    if (mp_node == null) {
        dprintk("[vfs] mountpoint \"%s\" not found!\n", mountpoint);
        return null;
    }

    if ((mp_node->attrs & node_attrs::TYPE_MASK) != node_attrs::DIR) {
        dprintk("[vfs] \"%s\" is not a folder!\n", mountpoint);
        return null;
    }

    let fs_mp: struct fs_mount* = kalloc<struct fs_mount>(1);

    fs_mp->mountpoint = kalloc<uint8>(strlen(mountpoint) + 1);
    strcpy(fs_mp->mountpoint, mountpoint);

    fs_mp->info = info;
    fs_mp->root = mp_node;
    fs_mp->read = read;
    fs_mp->write = write;

    if (device != null) {
        fs_mp->device = kalloc<uint8*>(strlen(device) + 1);
        strcpy(fs_mp->device, device);
    } else {
        fs_mp->device = null;
    }

    dict_set(&_vfs_mp_table, mountpoint, fs_mp);

    dprintk("[vfs] \"%s\" mounted successfully\n", mountpoint);

    return fs_mp;
}

/**
 * Looks up a mount entry by its exact mountpoint path.
 *
 * @param mountpoint: null-terminated mountpoint path (e.g. "/volumes/NO NAME")
 *
 * @return pointer to the matching fs_mount, or null if not found
 */
fn vfs_get_mountpoint(mountpoint: uint8*) -> struct fs_mount* {
    let value: struct fs_mount*;
    if (!dict_get(&_vfs_mp_table, mountpoint, &value))
        return null;

    return value;
}

/**
 * Removes the mount entry for mountpoint from the mount table, unlinks the mount's root node from
 * its parent folder via fs_remove_child, destroys the root subtree via fs_destroy_node, and frees
 * the mountpoint, device, and info (if non-null) fields.
 *
 * @param mountpoint: null-terminated mountpoint path to unmount
 *
 * @return 0 on success, -1 if no matching mount entry is found
 */
fn vfs_destroy_mountpoint(mountpoint: uint8*) -> int32 {
    let mp = vfs_get_mountpoint(mountpoint);
    if (mp == null) {
        dprintk("[vfs] mountpoint \"%s\" doesn't exist!\n", mountpoint);
        return -1;
    }

    // get parent
    let parent_ref = fs_get_child(mp->root, "..");
    if (parent_ref != null)
        fs_remove_child(parent_ref->child, mp->root->name);

    fs_destroy_node(mp->root);
    kdealloc(mp->mountpoint);
    if (mp->device != null)
        kdealloc(mp->device);
    if (mp->info != null)
        kdealloc(mp->info);
    kdealloc(mp);

    dprintk("[vfs] \"%s\" unmounted successfully\n", mountpoint);

    return 0;
}

/**
 * Resolves pathname via vfs_get_node_for_path and returns the matching fs_node.
 *
 * @param pathname: null-terminated absolute path to resolve
 *
 * @return pointer to the fs_node, or null if not found
 */
fn vfs_get_node_for_path(pathname: uint8*, root: struct fs_node*) -> struct fs_node* {
    let current = root;
    if (current == null)
        current = _vfs_root;

    let str: uint8[256];
    set_bytes(str, 0, 256);

    let i: uint64 = 0;
    let j: uint64 = 0;
    until (current == null) {
        // end of string
        if (pathname[i] == '\0') {
            if (j > 0)
                // pathname is of type "/abc/def", `current` holds "/abc" so we
                // need to find "def"
                current = fs_get_child(current, str);

            // pathname is of type "/abc/def/" which needs to be resolved to
            // "/abc/def" `current` already holds "/abc/def" so return it
            return current;
        }

        // dir separator
        if (pathname[i] == '/') {
            if (j > 0)
                current = fs_get_child(current, str);

            // reset buffer
            set_bytes(str, 0, 256);
            j = 0;
        } else {
            // store next character
            str[j] = pathname[i];
            j = j + 1;
        }

        i = i + 1;
    }

    // if current == null the node doesn't exist
    return null;
}

/**
 * Resolves pathname via vfs_get_node_for_path and returns node->file_size.
 *
 * @param pathname: null-terminated absolute path of the file
 *
 * @return file size in bytes, or 0 if the node is not found
 */
fn vfs_get_file_size(pathname: uint8*) -> uint64 {
    let node = vfs_get_node_for_path(pathname, null);

    if (node == null)
        return 0;

    return fs_get_node_file_size(node);
}

/**
 * Resolves pathname via vfs_get_node_for_path, reads node->mount for the covering mount, then dispatches to
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
fn vfs_read(pathname: uint8*, buffer: uint8*, count: uint64, offset: uint64) -> int64 {
    dprintk("[vfs] read(): file=\"%s\", buff=%p, count=%d, offset=%d\n",
            pathname, buffer, count, offset);

    let node = vfs_get_node_for_path(pathname, null);
    // dprintk("[vfs] node=%p\n", node);
    return fs_read(node, buffer, count, offset);
}

/**
 * Resolves pathname via vfs_get_node_for_path, reads node->mount for the covering mount, then dispatches to
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
fn vfs_write(pathname: uint8*, buffer: uint8*, count: uint64, offset: uint64) -> int64 {
    dprintk("[vfs] write(): file=\"%s\", buff=%p, count=%d, offset=%d\n",
            pathname, buffer, count, offset);

    let node = vfs_get_node_for_path(pathname, null);
    // dprintk("[vfs] node=%p\n", node);
    return fs_write(node, buffer, count, offset);
}


/**
 * Resolves path via vfs_get_node_for_path and creates a new subfolder named name inside it via
 * fs_add_subfolder.
 *
 * @param path:  null-terminated absolute path of the parent folder (e.g. "/volumes")
 * @param name:  null-terminated name for the new directory
 * @param attrs: attribute flags (FS_NODE_ATTRS_FLAG_*)
 * @param mount: fs_mount pointer stored in node->mount (void * to avoid circular include), or null
 *
 * @return pointer to the new folder node, or null if the parent is not found or creation fails
 */
fn vfs_create_dir(path: uint8*, name: uint8*, attrs: uint32, mount: struct fs_mount*) -> struct fs_node* {
    let parent = vfs_get_node_for_path(path, null);
    let node = fs_add_subfolder(parent, name, attrs, null, mount);
    if (node == null) {
        dprintk("[vfs] cannot create \"%s/%s\"!\n", path, name);
        return null;
    }

    return node;
}

/**
 * Resolves path via vfs_get_node_for_path and creates a new file named name inside it via
 * fs_add_file_to_folder.
 *
 * @param path:      null-terminated absolute path of the parent folder (e.g. "/volumes/NO NAME")
 * @param name:      null-terminated name for the new file
 * @param file_size: file size in bytes, stored in node->file_size
 * @param attrs:     attribute flags (FS_NODE_ATTRS_FLAG_*)
 * @param mount:     fs_mount pointer stored in node->mount (void * to avoid circular include), or
 *                   null
 *
 * @return pointer to the new file node, or null if the parent is not found or creation fails
 */
fn vfs_create_file(path: uint8*, name: uint8*, file_size: uint64, attrs: uint32, mount: struct fs_mount*) -> struct fs_node* {
    let parent = vfs_get_node_for_path(path, null);
    let node = fs_add_file_to_folder(parent, name, file_size, attrs, null, mount);
    if (node == null) {
        dprintk("[vfs] cannot create \"%s/%s\"!\n", path, name);
        return null;
    }

    return node;
}

/**
 * Prints the entire VFS tree to the kernel log via printk, starting from the global root's
 * children. Skips entering "." and ".." nodes to avoid cycles.
 */
fn vfs_dump_fs() {
    dprintk("[vfs] _vfs_root=%p &_vfs_root=%p\n", _vfs_root, &_vfs_root);
    fs_dump_node(_vfs_root->child, "");
}
