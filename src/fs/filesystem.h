#pragma once

#include <string.h>
#include <stdint.h>

#define FS_NODE_ATTRS_TYPE_MASK (1 << 0)
#define FS_NODE_ATTRS_TYPE_FOLDER 0
#define FS_NODE_ATTRS_TYPE_FILE 1

#define FS_NODE_ATTRS_FLAG_LINK (1 << 2)
#define FS_NODE_ATTRS_FLAG_HIDDEN (1 << 3)
#define FS_NODE_ATTRS_FLAG_READONLY (1 << 4)

#define FS_NODE_ATTRS_FLAG_MASK (FS_NODE_ATTRS_FLAG_LINK | FS_NODE_ATTRS_FLAG_HIDDEN | FS_NODE_ATTRS_FLAG_READONLY)

typedef int (*fs_handler_t)(char *, uint8_t *, size_t);

struct fs_node
{
    char *name;
    size_t size;
    uint64_t data;
    uint16_t attrs;
    struct fs_node *next;
    struct fs_node *child;
};

/**
 * Searches the direct children of node for a child whose name matches name.
 *
 * @param node: parent folder node to search
 * @param name: null-terminated name to match
 *
 * @return pointer to the matching child node, or NULL if not found
 */
struct fs_node *fs_get_child(struct fs_node *node, char *name);

/**
 * Unlinks the first direct child of node whose name matches name.
 * Does not free the removed node; the caller is responsible for it.
 *
 * @param node: parent folder node to search
 * @param name: null-terminated name of the child to remove
 *
 * @return 0 on success, -1 if no matching child is found
 */
int fs_remove_child(struct fs_node *node, char *name);

/**
 * Allocates and initializes a new fs_node on the heap.
 * The name is duplicated into a heap-allocated buffer via fs_node_rename.
 *
 * @param name:      null-terminated name string (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     attribute flags (FS_NODE_ATTRS_TYPE_* combined with FS_NODE_ATTRS_FLAG_*)
 * @param data:      driver-private value stored in the node's data field (e.g. FAT32 first cluster)
 * @param next:      next sibling node in the directory, or NULL
 * @param child:     first child node (for directories), or NULL
 *
 * @return pointer to the new node, or NULL if kmalloc failed
 */
struct fs_node *fs_create_node(char *name, size_t name_size, uint16_t attrs, uint64_t data, struct fs_node *next, struct fs_node *child);

/**
 * Allocates and initializes a new fs_node with FS_NODE_ATTRS_TYPE_FILE set.
 * Convenience wrapper around fs_create_node with next and child set to NULL.
 *
 * @param name:      null-terminated name string (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     additional attribute flags (FS_NODE_ATTRS_FLAG_*); type bits are ignored
 * @param data:      driver-private value stored in the node's data field
 *
 * @return pointer to the new file node, or NULL if kmalloc failed
 */
struct fs_node *fs_create_file(char *name, size_t name_size, uint16_t attrs, uint64_t data);

/**
 * Allocates and initializes a new fs_node with FS_NODE_ATTRS_TYPE_FOLDER set.
 * Convenience wrapper around fs_create_node with next and child set to NULL.
 * Automatically adds a "." self-reference child.
 *
 * @param name:      null-terminated name string (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     additional attribute flags (FS_NODE_ATTRS_FLAG_*); type bits are ignored
 * @param data:      driver-private value stored in the node's data field
 *
 * @return pointer to the new folder node, or NULL if kmalloc failed
 */
struct fs_node *fs_create_folder(char *name, size_t name_size, uint16_t attrs, uint64_t data);

/**
 * Appends node to the end of parent's child linked list. Does not check
 * that parent is a folder; callers must enforce that constraint.
 *
 * @param parent: folder node to append into
 * @param node:   node to append
 */
void fs_add_to_folder(struct fs_node *parent, struct fs_node *node);

/**
 * Creates a new file node and appends it to the child list of a folder node.
 * The new node is inserted after the last sibling in the folder's child chain.
 *
 * @param node:      pointer to the parent folder node
 * @param name:      null-terminated name string for the new file (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     attribute flags for the new file (FS_NODE_ATTRS_FLAG_*)
 * @param data:      driver-private value stored in the node's data field
 *
 * @return pointer to the new file node, or NULL if node is not a folder
 */
struct fs_node *fs_add_file_to_folder(struct fs_node *node, char *name, size_t name_size, uint16_t attrs, uint64_t data);

/**
 * Creates a new folder node and appends it to the child list of a folder node.
 * The new node is inserted after the last sibling in the folder's child chain.
 *
 * @param parent:      pointer to the parent folder node
 * @param name:      null-terminated name string for the new subfolder (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     attribute flags for the new subfolder (FS_NODE_ATTRS_FLAG_*)
 * @param data:      driver-private value stored in the node's data field
 *
 * @return pointer to the new folder node, or NULL if node is not a folder
 */
struct fs_node *fs_add_subfolder(struct fs_node *parent, char *name, size_t name_size, uint16_t attrs, uint64_t data);

/**
 * Replaces the name of an existing fs_node with a new name string.
 * The old name is freed via kfree; the new name is duplicated into a heap-allocated buffer.
 *
 * @param node:      pointer to the node to rename
 * @param name:      new null-terminated name string (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 */
void fs_node_rename(struct fs_node *node, char *name, size_t name_size);

/**
 * Recursively frees an fs_node and its entire subtree (child and next chains).
 * Frees the name and the node itself at each level.
 *
 * @param node: root of the subtree to free
 */
void fs_destroy_node(struct fs_node *node);

/**
 * Recursively prints the subtree rooted at node to the kernel log via printk.
 * Each entry is printed as "prefix/name". Descends into folder children but
 * skips recursing into "." and ".." to avoid cycles. Advances through siblings
 * via node->next at each level.
 *
 * @param node:   pointer to the starting fs_node
 * @param prefix: path prefix to prepend to each entry (may be empty string or NULL)
 */
void fs_dump_node(struct fs_node *node, char *prefix);