#pragma once

#include <string.h>
#include <stdint.h>

#define FS_NODE_ATTRS_TYPE_MASK 3
#define FS_NODE_ATTRS_TYPE_FOLDER 0
#define FS_NODE_ATTRS_TYPE_FILE 1

#define FS_NODE_ATTRS_FLAG_LINK (1 << 2)
#define FS_NODE_ATTRS_FLAG_HIDDEN (1 << 3)
#define FS_NODE_ATTRS_FLAG_READONLY (1 << 4)

#define FS_NODE_ATTRS_FLAG_MASK (FS_NODE_ATTRS_FLAG_LINK | FS_NODE_ATTRS_FLAG_HIDDEN | FS_NODE_ATTRS_FLAG_READONLY)

struct fs_node
{
    char *name;
    size_t size;
    uint16_t attrs;
    struct fs_node *next;
    struct fs_node *child;
};

/**
 * Allocates and initializes a new fs_node on the heap.
 * The name is duplicated into a heap-allocated buffer via fs_node_rename.
 *
 * @param name:      null-terminated name string (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     attribute flags (FS_NODE_ATTRS_TYPE_* combined with FS_NODE_ATTRS_FLAG_*)
 * @param next:      next sibling node in the directory, or NULL
 * @param child:     first child node (for directories), or NULL
 *
 * @return pointer to the new node, or NULL if kmalloc failed
 */
struct fs_node *fs_create_node(char *name, size_t name_size, uint16_t attrs, struct fs_node *next, struct fs_node *child);

/**
 * Allocates and initializes a new fs_node with FS_NODE_ATTRS_TYPE_FILE set.
 * Convenience wrapper around fs_create_node with next and child set to NULL.
 *
 * @param name:      null-terminated name string (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     additional attribute flags (FS_NODE_ATTRS_FLAG_*); type bits are ignored
 *
 * @return pointer to the new file node, or NULL if kmalloc failed
 */
struct fs_node *fs_create_file(char *name, size_t name_size, uint16_t attrs);

/**
 * Allocates and initializes a new fs_node with FS_NODE_ATTRS_TYPE_FOLDER set.
 * Convenience wrapper around fs_create_node with next and child set to NULL.
 *
 * @param name:      null-terminated name string (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     additional attribute flags (FS_NODE_ATTRS_FLAG_*); type bits are ignored
 *
 * @return pointer to the new folder node, or NULL if kmalloc failed
 */
struct fs_node *fs_create_folder(char *name, size_t name_size, uint16_t attrs);

/**
 * Creates a new file node and appends it to the child list of a folder node.
 * The new node is inserted after the last sibling in the folder's child chain.
 *
 * @param node:      pointer to the parent folder node
 * @param name:      null-terminated name string for the new file (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     attribute flags for the new file (FS_NODE_ATTRS_FLAG_*)
 *
 * @return 0 on success, non-zero if kmalloc failed
 */
int fs_add_file_to_folder(struct fs_node *node, char *name, size_t name_size, uint16_t attrs);

/**
 * Creates a new folder node and appends it to the child list of a folder node.
 * The new node is inserted after the last sibling in the folder's child chain.
 *
 * @param node:      pointer to the parent folder node
 * @param name:      null-terminated name string for the new subfolder (duplicated into a heap-allocated buffer)
 * @param name_size: maximum number of characters to copy from name (excluding null terminator)
 * @param attrs:     attribute flags for the new subfolder (FS_NODE_ATTRS_FLAG_*)
 *
 * @return 0 on success, non-zero if kmalloc failed
 */
int fs_add_subfolder(struct fs_node *node, char *name, size_t name_size, uint16_t attrs);

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
 * Frees a previously allocated fs_node. Does not recursively free next or child.
 *
 * @param node: pointer to the node to free
 */
void fs_destroy_node(struct fs_node *node);

/**
 * Prints the full subtree rooted at node to the kernel log via printk.
 * Each entry is printed as "prefix/name", with folders showing their
 * "." and ".." entries followed by their children recursively.
 *
 * @param node: pointer to the root fs_node to dump
 */
void fs_dump_node(struct fs_node *node);