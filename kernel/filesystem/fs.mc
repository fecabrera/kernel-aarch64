import "debug";
import "memory";
import "stack";
import "libc/stdio";
import "libc/string";

const FS_NODE_ATTRS_TYPE_MASK = (1 << 0);
const FS_NODE_ATTRS_TYPE_FOLDER = 0;
const FS_NODE_ATTRS_TYPE_FILE = 1;

const FS_NODE_ATTRS_FLAG_LINK = (1 << 2);
const FS_NODE_ATTRS_FLAG_HIDDEN = (1 << 3);

const FS_NODE_ATTRS_PERMISSIONS_READ = (1 << 4);
const FS_NODE_ATTRS_PERMISSIONS_WRITE = (1 << 5);
const FS_NODE_ATTRS_PERMISSIONS_EXECUTE = (1 << 6);

const FS_NODE_ATTRS_PERMISSIONS_MASK = (FS_NODE_ATTRS_PERMISSIONS_READ |
                                        FS_NODE_ATTRS_PERMISSIONS_WRITE |
                                        FS_NODE_ATTRS_PERMISSIONS_EXECUTE);

const FS_NODE_ATTRS_FLAG_MASK = (FS_NODE_ATTRS_FLAG_LINK |
                                 FS_NODE_ATTRS_FLAG_HIDDEN);

const FS_NODE_ATTRS_FLAG_PERMISSIONS_MASK = (FS_NODE_ATTRS_PERMISSIONS_MASK |
                                             FS_NODE_ATTRS_FLAG_MASK);

const FS_IO_ERROR_FILE_NOT_FOUND = -1;
const FS_IO_ERROR_NOT_A_FILE = -2;
const FS_IO_ERROR_MOUNTPOINT_NOT_FOUND = -3;
const FS_IO_ERROR_HANDLER_NOT_PROVIDED = -4;
const FS_IO_ERROR_NOT_PERMITTED = -5;

/**
 * A single node in the VFS tree, representing a file or folder. Folders link
 * their entries through the `child`/`next` chain; every folder also carries
 * "." and ".." entries (see fs_create_self_ref / fs_create_parent_ref).
 *
 * @field name:      heap-allocated, NUL-terminated node name (owned; freed on destroy)
 * @field file_size: size in bytes for files; 0 for folders
 * @field attrs:     type bit (FS_NODE_ATTRS_TYPE_*) OR'd with flags (FS_NODE_ATTRS_FLAG_*)
 * @field info:      driver-private pointer (e.g. fat32_entry_reference *), or null
 * @field mount:     covering mountpoint for this node, or null
 * @field next:      next sibling in the parent folder, or null
 * @field child:     first child (folders only), or null
 */
struct fs_node {
    name: uint8*;
    file_size: uint64;
    attrs: uint32;
    info: uint8*;
    mount: struct fs_mount*;
    next: struct fs_node*;
    child: struct fs_node*;
}

/**
 * A mounted filesystem registered at a VFS path. fs_read/fs_write dispatch a
 * node's I/O through this mount's read/write handlers.
 *
 * @field mountpoint: VFS path this mount is registered under (owned, heap-allocated)
 * @field device:     VFS path of the underlying block device, or null
 * @field info:       filesystem-private superblock data (heap-allocated; freed by
 *                    vfs_destroy_mountpoint), or null
 * @field root:       VFS node the mount covers
 * @field read:       read handler for this mount, or null
 * @field write:      write handler for this mount, or null
 */
struct fs_mount {
    mountpoint: uint8*;
    device: uint8*;
    info: uint8*;
    root: struct fs_node*;
    read: fn (struct fs_node*, uint8*, uint64, uint64) -> int64;
    write: fn (struct fs_node*, uint8*, uint64, uint64) -> int64;
}

/**
 * Searches the direct children of node for a child whose name matches name.
 *
 * @param node: parent folder node to search
 * @param name: null-terminated name to match
 *
 * @return pointer to the matching child node, or null if not found
 */
fn fs_get_child(node: struct fs_node*, name: uint8*) -> struct fs_node* {
    let current = node->child;
    until (current == null) {
        if (strcmp(current->name, name) == 0)
            return current;

        current = current->next;
    }
    return null;
}

/**
 * Unlinks the first direct child of node whose name matches name.
 * Does not free the removed node; the caller is responsible for it.
 *
 * @param node: parent folder node to search
 * @param name: null-terminated name of the child to remove
 *
 * @return 0 on success, -1 if no matching child is found
 */
fn fs_remove_child(node: struct fs_node*, name: uint8*) -> int32 {
    let prev: struct fs_node* = null;
    let current: struct fs_node* = node->child;
    until (current == null) {
        if (strcmp(current->name, name) == 0) {
            if (prev != null)
                prev->next = current->next;
            else
                node->child = current->next;

            return 0;
        }

        prev = current;
        current = current->next;
    }
    return -1;
}

/**
 * Allocates and initializes a new fs_node on the heap.
 * The name is duplicated into a heap-allocated buffer via fs_node_rename.
 *
 * @param name:      null-terminated name string (length determined via strlen; duplicated into
 * heap)
 * @param file_size: file size in bytes stored in node->file_size (0 for folders)
 * @param attrs:     attribute flags (FS_NODE_ATTRS_TYPE_* combined with FS_NODE_ATTRS_FLAG_*)
 * @param info:      driver-private pointer stored in node->info (e.g. fat32_entry_reference *)
 * @param mount:     fs_mount pointer stored in node->mount, or null
 * @param next:      next sibling node in the directory, or null
 * @param child:     first child node (for directories), or null
 *
 * @return pointer to the new node, or null if alloc failed
 */
fn fs_create_node(name: uint8*, file_size: uint64, attrs: uint32, info: uint8*,
                  mount: struct fs_mount*, next: struct fs_node*, child: struct fs_node*) -> struct fs_node * {
    let node: struct fs_node* = alloc<struct fs_node>(1);

    node->name = null;
    node->file_size = file_size;
    node->attrs = attrs;
    node->info = info;
    node->mount = mount;
    node->next = next;
    node->child = child;

    if (name != null)
        fs_node_rename(node, name);

    return node;
}

/**
 * Recursively frees an fs_node and its entire subtree (child and next chains).
 * Frees the name and the node itself at each level.
 *
 * @param node: root of the subtree to free
 */
fn fs_destroy_node(node: struct fs_node*) {
    if (node == null)
        return;

    if (strcmp(node->name, ".") != 0 and strcmp(node->name, "..") != 0 and node->child != null)
        fs_destroy_node(node->child);

    if (node->next != null)
        fs_destroy_node(node->next);

    dealloc(node->name);
    dealloc(node);
}

/**
 * Allocates and initializes a new fs_node with FS_NODE_ATTRS_TYPE_FILE set.
 * Convenience wrapper around fs_create_node with next and child set to null.
 *
 * @param name:      null-terminated name string (length determined via strlen; duplicated into
 * heap)
 * @param file_size: file size in bytes stored in node->file_size
 * @param attrs:     additional attribute flags (FS_NODE_ATTRS_FLAG_*); type bits are ignored
 * @param data:      driver-private pointer stored in node->info
 * @param mount:     fs_mount pointer stored in node->mount, or null
 *
 * @return pointer to the new file node, or null if alloc failed
 */
fn fs_create_file(name: uint8*, file_size: uint64, attrs: uint32, data: uint8*,
                  mount: struct fs_mount*) -> struct fs_node * {
    let _attrs = (attrs & FS_NODE_ATTRS_FLAG_PERMISSIONS_MASK) | FS_NODE_ATTRS_TYPE_FILE;
    return fs_create_node(name, file_size, _attrs, data, mount, null, null);
}

/**
 * Allocates and initializes a new fs_node with FS_NODE_ATTRS_TYPE_FOLDER set.
 * Convenience wrapper around fs_create_node with next and child set to null.
 * Automatically adds a "." self-reference child.
 *
 * @param name:  null-terminated name string (length determined via strlen; duplicated into heap)
 * @param attrs: additional attribute flags (FS_NODE_ATTRS_FLAG_*); type bits are ignored
 * @param data:  driver-private pointer stored in node->info
 * @param mount: fs_mount pointer stored in node->mount, or null
 *
 * @return pointer to the new folder node, or null if alloc failed
 */
fn fs_create_folder(name: uint8*, attrs: uint32, data: uint8*, mount: struct fs_mount*) -> struct fs_node * {
    let _attrs = (attrs & FS_NODE_ATTRS_FLAG_PERMISSIONS_MASK) | FS_NODE_ATTRS_TYPE_FOLDER;

    let folder = fs_create_node(name, 0, _attrs, data, mount, null, null);
    fs_create_self_ref(folder);

    return folder;
}

/**
 * Appends node to the end of parent's child linked list. Does not check
 * that parent is a folder; callers must enforce that constraint.
 *
 * @param parent: folder node to append into
 * @param node:   node to append
 */
fn fs_add_to_folder(parent: struct fs_node*, node: struct fs_node*) {
    let current = parent->child;
    if (current != null) {
        until (current->next == null)
            current = current->next;

        current->next = node;
    } else {
        parent->child = node;
    }
}

/**
 * Adds the "." self-reference entry to a freshly created folder. The entry's
 * child points back at the folder itself. Internal; called by fs_create_folder.
 *
 * @param folder: folder node to add the "." entry to
 *
 * @return pointer to the new "." node
 */
@private
fn fs_create_self_ref(folder: struct fs_node*) -> struct fs_node* {
    let attrs: uint32 = ((folder->attrs & FS_NODE_ATTRS_PERMISSIONS_MASK) |
                         FS_NODE_ATTRS_TYPE_FOLDER |
                         FS_NODE_ATTRS_FLAG_LINK |
                         FS_NODE_ATTRS_FLAG_HIDDEN);
    let self_ref = fs_create_node(".", 0, attrs, null, folder->mount, null, null);
    self_ref->child = folder;
    fs_add_to_folder(folder, self_ref);
    return self_ref;
}

/**
 * Adds the ".." parent-reference entry to a folder. The entry's child points
 * at the parent folder. Internal; called by fs_add_subfolder.
 *
 * @param parent: parent folder the ".." entry should point to
 * @param folder: folder node to add the ".." entry to
 *
 * @return pointer to the new ".." node
 */
@private
fn fs_create_parent_ref(parent: struct fs_node*, folder: struct fs_node*) -> struct fs_node* {
    let attrs: uint32 = ((parent->attrs & FS_NODE_ATTRS_PERMISSIONS_MASK) |
                         FS_NODE_ATTRS_TYPE_FOLDER |
                         FS_NODE_ATTRS_FLAG_LINK |
                         FS_NODE_ATTRS_FLAG_HIDDEN);
    let parent_ref = fs_create_node("..", 0, attrs, null, folder->mount, null, null);
    parent_ref->child = parent;
    fs_add_to_folder(folder, parent_ref);
    return parent_ref;
}

/**
 * Creates a new file node and appends it to the child list of a folder node.
 * The new node is inserted after the last sibling in the folder's child chain.
 *
 * @param node:      pointer to the parent folder node
 * @param name:      null-terminated name string for the new file (duplicated into a heap-allocated
 * buffer)
 * @param file_size: file size in bytes, stored in node->file_size
 * @param attrs:     attribute flags for the new file (FS_NODE_ATTRS_FLAG_*)
 * @param data:      driver-private pointer stored in node->info
 * @param mount:     fs_mount pointer stored in node->mount, or null
 *
 * @return pointer to the new file node, or null if node is not a folder
 */
fn fs_add_file_to_folder(parent: struct fs_node*, name: uint8*, file_size: uint64,
                         attrs: uint32, data: uint8*, mount: struct fs_mount*) -> struct fs_node* {
    if ((parent->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
        dprintk("[filesystem] Node is not a folder!\n");
        return null;
    }

    let file = fs_create_file(name, file_size, attrs, data, mount);
    file->file_size = file_size;
    fs_add_to_folder(parent, file);
    return file;
}

/**
 * Creates a new folder node and appends it to the child list of a folder node.
 * The new node is inserted after the last sibling in the folder's child chain.
 *
 * @param parent: pointer to the parent folder node
 * @param name:   null-terminated name string for the new subfolder (duplicated into a
 * heap-allocated buffer)
 * @param attrs:  attribute flags for the new subfolder (FS_NODE_ATTRS_FLAG_*)
 * @param data:   driver-private pointer stored in node->info
 * @param mount:  fs_mount pointer stored in node->mount, or null
 *
 * @return pointer to the new folder node, or null if node is not a folder
 */
fn fs_add_subfolder(parent: struct fs_node *, name: uint8*, attrs: uint32, data: uint8*,
                    mount: struct fs_mount*) -> struct fs_node * {
    if ((parent->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
        dprintk("[filesystem] node is not a folder!\n");
        return null;
    }

    let folder = fs_create_folder(name, attrs, data, mount);
    fs_create_parent_ref(parent, folder);
    fs_add_to_folder(parent, folder);
    return folder;
}

/**
 * Returns node->file_size.
 *
 * @param node: file node to query
 *
 * @return file size in bytes
 */
fn fs_get_node_file_size(node: struct fs_node*) -> uint64 {
    return node->file_size;
}

/**
 * Replaces the name of an existing fs_node with a new name string.
 * The old name is freed via dealloc; the new name is duplicated into a heap-allocated buffer.
 *
 * @param node:      pointer to the node to rename
 * @param name:      new null-terminated name string (duplicated into a heap-allocated buffer)
 */
fn fs_node_rename(node: struct fs_node*, name: uint8*) {
    if (node->name != null)
        dealloc(node->name);

    let _name_size: uint64 = strlen(name);
    let _name: uint8* = alloc<uint8>(_name_size + 1);
    strncpy(_name, name, _name_size);
    _name[_name_size] = '\0';

    node->name = _name;
}

/**
 * Prints a single node as "prefix/name" to the kernel log. Does nothing if the
 * node has no name. Internal helper for fs_dump_node.
 *
 * @param node:   node to print
 * @param prefix: path prefix to prepend, or null for no prefix
 */
@private
fn fs_dump_file(node: struct fs_node*, prefix: uint8*) {
    if (node->name == null)
        return;

    if (prefix != null)
        printk("%s/", prefix);

    printk("%s\n", node->name);
}

/**
 * Recursively prints the subtree rooted at node to the kernel log via printk.
 * Each entry is printed as "prefix/name". Descends into folder children but  skips recursing into
 * "." and ".." to avoid cycles. Advances through siblings via node->next at each level.
 *
 * @param node:   pointer to the starting fs_node
 * @param prefix: path prefix to prepend to each entry (may be empty string or null)
 */
fn fs_dump_node(node: struct fs_node*, prefix: uint8*) {
    if (node == null)
        return;

    dprintk("[dump] node=%p name=%p first=%p\n", node, node->name, node->name);

    fs_dump_file(node, prefix);

    let attrs = node->attrs & FS_NODE_ATTRS_TYPE_MASK;
    if (attrs == FS_NODE_ATTRS_TYPE_FOLDER and
        strcmp(node->name, ".") != 0 and
        strcmp(node->name, "..") != 0) {
        let _prefix: uint8*;

        if (node->name != null and prefix != null) {
            let _strlen: uint64 = strlen(prefix) + strlen(node->name) + 2;
            _prefix = alloc<uint8*>(_strlen);
            sprintf(_prefix, "%s/%s", prefix, node->name);
        } else if (prefix != null) {
            _prefix = prefix;
        } else {
            _prefix = node->name;
        }

        if (node->child != null)
            fs_dump_node(node->child, _prefix);

        if (prefix != null and node->name != null)
            dealloc(_prefix);
    }

    if (node->next != null)
        fs_dump_node(node->next, prefix);
}

/**
 * Reads from a node by dispatching to its covering mount's read handler.
 *
 * @param node:   node to read from (must be non-null and have a mount with a read handler)
 * @param buffer: output buffer
 * @param count:  number of bytes to read
 * @param offset: byte offset into the file to read from
 *
 * @return return value of the mount's read handler on success;
 *         FS_IO_ERROR_FILE_NOT_FOUND if node is null,
 *         FS_IO_ERROR_MOUNTPOINT_NOT_FOUND if the node has no mount,
 *         FS_IO_ERROR_HANDLER_NOT_PROVIDED if the mount has no read handler
 */
fn fs_read(node: struct fs_node*, buffer: uint8*, count: uint64, offset: uint64) -> int64 {
    if (node == null) {
        dprintk("[fs] node is null!\n");
        return FS_IO_ERROR_FILE_NOT_FOUND;
    }

    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FILE) {
        dprintk("[fs] \"%s\" is not a file!\n", node->name);
        return FS_IO_ERROR_NOT_A_FILE;
    }

    let mp = node->mount;
    if (mp == null) {
        dprintk("[fs] mountpoint for \"%s\" not found!\n", node->name);
        return FS_IO_ERROR_MOUNTPOINT_NOT_FOUND;
    }

    if (mp->read == null) {
        dprintk("[fs] mountpoint \"%s\" didn't provide a `read` handler!\n", mp->mountpoint);
        return FS_IO_ERROR_HANDLER_NOT_PROVIDED;
    }

    if ((node->attrs & FS_NODE_ATTRS_PERMISSIONS_READ) == 0) {
        dprintk("[fs] \"%s\" does not allow reads\n", node->name);
        return FS_IO_ERROR_NOT_PERMITTED;
    }

    return mp->read(node, buffer, count, offset);
}

/**
 * Writes to a node by dispatching to its covering mount's write handler.
 *
 * @param node:   node to write to (must be non-null and have a mount with a write handler)
 * @param buffer: input buffer
 * @param count:  number of bytes to write
 * @param offset: byte offset into the file to write to
 *
 * @return return value of the mount's write handler on success;
 *         FS_IO_ERROR_FILE_NOT_FOUND if node is null,
 *         FS_IO_ERROR_MOUNTPOINT_NOT_FOUND if the node has no mount,
 *         FS_IO_ERROR_HANDLER_NOT_PROVIDED if the mount has no write handler
 */
fn fs_write(node: struct fs_node*, buffer: uint8*, count: uint64, offset: uint64) -> int64 {
    if (node == null) {
        dprintk("[fs] node is null!\n");
        return FS_IO_ERROR_FILE_NOT_FOUND;
    }

    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FILE) {
        dprintk("[fs] \"%s\" is not a file!\n", node->name);
        return FS_IO_ERROR_NOT_A_FILE;
    }

    let mp = node->mount;
    if (mp == null) {
        dprintk("[fs] mountpoint for file \"%s\" not found!\n", node->name);
        return FS_IO_ERROR_MOUNTPOINT_NOT_FOUND;
    }

    if (mp->write == null) {
        dprintk("[fs] mountpoint \"%s\" didn't provide a `write` handler!\n", mp->mountpoint);
        return FS_IO_ERROR_HANDLER_NOT_PROVIDED;
    }

    if ((node->attrs & FS_NODE_ATTRS_PERMISSIONS_WRITE) == 0) {
        dprintk("[fs] \"%s\" does not allow writes\n", node->name);
        return FS_IO_ERROR_NOT_PERMITTED;
    }

    return mp->write(node, buffer, count, offset);
}

/**
 * Builds the absolute directory path containing node into buf. Walks up the
 * ".." chain to the root, pushing each folder ancestor (non-folder nodes are
 * skipped) onto a stack, then pops them back down, writing each name followed
 * by '/'. The result always begins with '/', ends with a trailing '/', and is
 * null-terminated; unnamed nodes (e.g. the root) contribute only their separator.
 *
 * @param node: node whose containing directory path is resolved
 * @param buf:  output buffer for the null-terminated path
 * @param size: capacity of buf in bytes
 *
 * @return the path length excluding the null terminator, or -1 if the path
 *         would not fit in size bytes
 */
fn fs_get_absolute_dir(node: struct fs_node*, buf: uint8*, size: uint64) -> int64 {
    let s: struct stack<struct fs_node*>;
    stack_init(&s, 10);
    defer stack_destroy(&s);

    let current = node;
    until (current == null) {
        dprintk("[fs] pushing \"%s\"\n", current->name == null ? "" : current->name);

        if ((current->attrs & FS_NODE_ATTRS_TYPE_MASK == FS_NODE_ATTRS_TYPE_FOLDER))
            stack_push(&s, current);

        let parent_ref = fs_get_child(current, "..");
        current = parent_ref != null ? parent_ref->child : null;
    }

    let length: uint64 = 0;
    buf[length] = '/';
    length = length + 1;
    until(stack_is_empty(&s)) {
        current = stack_pop(&s);

        dprintk("[fs] popped \"%s\"\n", current->name == null ? "" : current->name);

        if (current->name == null) continue;
            
        let count = strlen(current->name);
        if (length + count > size)
            return -1; // won't copy otherwise it'd overflow
        
        bytecopy(&buf[length], current->name, count);
        length = length + count;

        buf[length] = '/';
        length = length + 1;
    }
    
    buf[length] = '\0';
    
    return length as int64;
}
