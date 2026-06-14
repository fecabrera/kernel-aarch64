const FS_NODE_ATTRS_TYPE_MASK = (1 << 0);
const FS_NODE_ATTRS_TYPE_FOLDER = 0;
const FS_NODE_ATTRS_TYPE_FILE = 1;

const FS_NODE_ATTRS_FLAG_LINK = (1 << 2);
const FS_NODE_ATTRS_FLAG_HIDDEN = (1 << 3);
const FS_NODE_ATTRS_FLAG_READONLY = (1 << 4);

const FS_NODE_ATTRS_FLAG_MASK = (FS_NODE_ATTRS_FLAG_LINK | FS_NODE_ATTRS_FLAG_HIDDEN | FS_NODE_ATTRS_FLAG_READONLY);

struct fs_node {
    name: uint8*;
    file_size: uint64;
    attrs: uint16;
    info: uint8*;             // driver-private pointer (e.g. fat32_entry_reference *)
    mount: struct fs_mount*; // vfs_mount * for this node's covering mountpoint, or NULL
    next: struct fs_node*;
    child: struct fs_node*;
}

struct fs_mount {
    mountpoint: uint8*; // VFS path this mount is registered under
    device: uint8*;     // VFS path of the underlying block device, or NULL
    info: uint8*;       // filesystem-private superblock data (heap-allocated; freed by vfs_destroy_mountpoint)
    root: struct fs_node*;
    read: fn (struct fs_node*, uint8*, uint64, uint64) -> int32;
    write: fn (struct fs_node*, uint8*, uint64, uint64) -> int32;
}

/**
 * Searches the direct children of node for a child whose name matches name.
 *
 * @param node: parent folder node to search
 * @param name: null-terminated name to match
 *
 * @return pointer to the matching child node, or NULL if not found
 */
@extern fn fs_get_child(node: struct fs_node*, name: uint8*) -> struct fs_node*;

/**
 * Unlinks the first direct child of node whose name matches name.
 * Does not free the removed node; the caller is responsible for it.
 *
 * @param node: parent folder node to search
 * @param name: null-terminated name of the child to remove
 *
 * @return 0 on success, -1 if no matching child is found
 */
@extern fn fs_remove_child(node: struct fs_node*, name: uint8*) -> int32;

/**
 * Allocates and initializes a new fs_node on the heap.
 * The name is duplicated into a heap-allocated buffer via fs_node_rename.
 *
 * @param name:      null-terminated name string (length determined via strlen; duplicated into
 * heap)
 * @param file_size: file size in bytes stored in node->file_size (0 for folders)
 * @param attrs:     attribute flags (FS_NODE_ATTRS_TYPE_* combined with FS_NODE_ATTRS_FLAG_*)
 * @param info:      driver-private pointer stored in node->info (e.g. fat32_entry_reference *)
 * @param mount:     vfs_mount pointer stored in node->mount (void * to avoid circular include), or
 * NULL
 * @param next:      next sibling node in the directory, or NULL
 * @param child:     first child node (for directories), or NULL
 *
 * @return pointer to the new node, or NULL if kmalloc failed
 */
@extern fn fs_create_node(name: uint8*, file_size: uint64, attrs: uint16, info: uint8*,
                          mount: struct fs_mount*, next: struct fs_node*,
                          child: struct fs_node*) -> struct fs_node *;

/**
 * Allocates and initializes a new fs_node with FS_NODE_ATTRS_TYPE_FILE set.
 * Convenience wrapper around fs_create_node with next and child set to NULL.
 *
 * @param name:      null-terminated name string (length determined via strlen; duplicated into
 * heap)
 * @param file_size: file size in bytes stored in node->file_size
 * @param attrs:     additional attribute flags (FS_NODE_ATTRS_FLAG_*); type bits are ignored
 * @param data:      driver-private pointer stored in node->info
 * @param mount:     vfs_mount pointer stored in node->mount, or NULL
 *
 * @return pointer to the new file node, or NULL if kmalloc failed
 */
@extern fn fs_create_file(name: uint8*, file_size: uint64, attrs: uint16, data: uint8*,
                          mount: struct fs_mount*) -> struct fs_node *;

/**
 * Allocates and initializes a new fs_node with FS_NODE_ATTRS_TYPE_FOLDER set.
 * Convenience wrapper around fs_create_node with next and child set to NULL.
 * Automatically adds a "." self-reference child.
 *
 * @param name:  null-terminated name string (length determined via strlen; duplicated into heap)
 * @param attrs: additional attribute flags (FS_NODE_ATTRS_FLAG_*); type bits are ignored
 * @param data:  driver-private pointer stored in node->info
 * @param mount: vfs_mount pointer stored in node->mount, or NULL
 *
 * @return pointer to the new folder node, or NULL if kmalloc failed
 */
@extern fn fs_create_folder(name: uint8*, attrs: uint16, data: uint8*,
                            mount: struct fs_mount*) -> struct fs_node *;

/**
 * Appends node to the end of parent's child linked list. Does not check
 * that parent is a folder; callers must enforce that constraint.
 *
 * @param parent: folder node to append into
 * @param node:   node to append
 */
@extern fn fs_add_to_folder(parent: struct fs_node*, node: struct fs_node*);

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
 * @param mount:     vfs_mount pointer stored in node->mount, or NULL
 *
 * @return pointer to the new file node, or NULL if node is not a folder
 */
@extern fn fs_add_file_to_folder(node: struct fs_node*, name: uint8*, file_size: uint64,
                                 attrs: uint16, data: uint8*, mount: struct fs_mount*) -> struct fs_node*;

/**
 * Creates a new folder node and appends it to the child list of a folder node.
 * The new node is inserted after the last sibling in the folder's child chain.
 *
 * @param parent: pointer to the parent folder node
 * @param name:   null-terminated name string for the new subfolder (duplicated into a
 * heap-allocated buffer)
 * @param attrs:  attribute flags for the new subfolder (FS_NODE_ATTRS_FLAG_*)
 * @param data:   driver-private pointer stored in node->info
 * @param mount:  vfs_mount pointer stored in node->mount, or NULL
 *
 * @return pointer to the new folder node, or NULL if node is not a folder
 */
@extern fn fs_add_subfolder(parent: struct fs_node *, name: uint8*, attrs: uint16, data: uint8*,
                            mount: uint8*) -> struct fs_node *;

/**
 * Returns node->file_size.
 *
 * @param node: file node to query
 *
 * @return file size in bytes
 */
@extern fn fs_get_node_file_size(node: struct fs_node*) -> uint64;

/**
 * Replaces the name of an existing fs_node with a new name string.
 * The old name is freed via kfree; the new name is duplicated into a heap-allocated buffer.
 *
 * @param node:      pointer to the node to rename
 * @param name:      new null-terminated name string (duplicated into a heap-allocated buffer)
 */
@extern fn fs_node_rename(node: struct fs_node*, name: uint8*);

/**
 * Recursively frees an fs_node and its entire subtree (child and next chains).
 * Frees the name and the node itself at each level.
 *
 * @param node: root of the subtree to free
 */
@extern fn fs_destroy_node(node: struct fs_node*);

/**
 * Recursively prints the subtree rooted at node to the kernel log via printk.
 * Each entry is printed as "prefix/name". Descends into folder children but  skips recursing into
 * "." and ".." to avoid cycles. Advances through siblings via node->next at each level.
 *
 * @param node:   pointer to the starting fs_node
 * @param prefix: path prefix to prepend to each entry (may be empty string or NULL)
 */
@extern fn fs_dump_node(node: struct fs_node*, prefix: uint8*);