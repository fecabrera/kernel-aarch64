#include <debug.h>
#include <stdlib.h>
#include <mm/heap.h>
#include "filesystem.h"

struct fs_node *fs_create_node(char *name, size_t name_size, uint16_t attrs, struct fs_node *next, struct fs_node *child)
{
    struct fs_node *node = (struct fs_node *)kmalloc(sizeof(struct fs_node));

    node->name = NULL;
    node->attrs = attrs;
    node->next = next;
    node->child = child;

    fs_node_rename(node, name, name_size);

    return node;
}

struct fs_node *fs_create_file(char *name, size_t name_size, uint16_t attrs)
{
    size_t _name_size = strnlen(name, name_size);
    uint16_t _attrs = (attrs & FS_NODE_ATTRS_FLAG_MASK) | FS_NODE_ATTRS_TYPE_FILE;

    return fs_create_node(name, _name_size, _attrs, NULL, NULL);
}

struct fs_node *fs_create_folder(char *name, size_t name_size, uint16_t attrs)
{
    size_t _name_size = strnlen(name, name_size);
    uint16_t _attrs = (attrs & FS_NODE_ATTRS_FLAG_MASK) | FS_NODE_ATTRS_TYPE_FOLDER;

    struct fs_node *folder = fs_create_node(name, _name_size, _attrs, NULL, NULL);
    struct fs_node *self_ref = fs_create_node(".", 1, FS_NODE_ATTRS_TYPE_FOLDER, NULL, NULL);
    struct fs_node *parent_ref = fs_create_node("..", 2, FS_NODE_ATTRS_TYPE_FOLDER, NULL, NULL);

    folder->child = self_ref;
    self_ref->next = parent_ref;
    parent_ref->child = folder;

    return folder;
}

int fs_add_file_to_folder(struct fs_node *node, char *name, size_t name_size, uint16_t attrs)
{
    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER)
    {
        dprintk("[filesystem] Node is not a folder!\r\n");
        return -1;
    }

    struct fs_node *current = node->child;
    while (current->next)
        current = current->next;

    current->next = fs_create_file(name, name_size, attrs);
    return 0;
}

int fs_add_subfolder(struct fs_node *node, char *name, size_t name_size, uint16_t attrs)
{
    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER)
    {
        dprintk("[filesystem] Node is not a folder!\r\n");
        return -1;
    }

    struct fs_node *current = node->child;
    while (current->next)
        current = current->next;

    current->next = fs_create_folder(name, name_size, attrs);
    return 0;
}

void fs_node_rename(struct fs_node *node, char *name, size_t name_size)
{
    if (node->name)
        kfree(node->name);

    size_t _name_size = strnlen(name, name_size);
    char *_name = (char *)kmalloc(_name_size + 1);
    strncpy(_name, name, _name_size);
    _name[_name_size] = '\0';

    node->name = _name;
}

void fs_destroy_node(struct fs_node *node)
{
    kfree(node->name);
    kfree(node);
}

static void _fs_dump_file(struct fs_node *node, char *prefix)
{
    if (prefix != NULL)
        printk("%s/", prefix);

    printk("%s\r\n", node->name);
}

static void _fs_dump_node(struct fs_node *node, char *prefix)
{
    _fs_dump_file(node, prefix);

    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) == FS_NODE_ATTRS_TYPE_FOLDER)
    {
        char *_prefix;
        if (prefix)
        {
            size_t _strlen = strlen(prefix) + strlen(node->name) + 2;
            _prefix = (char *)kmalloc(_strlen);
            sprintf(_prefix, "%s/%s", prefix, node->name);
        }
        else
        {
            _prefix = node->name;
        }

        struct fs_node *current = node->child;

        _fs_dump_file(current, _prefix);
        current = current->next;
        _fs_dump_file(current, _prefix);
        current = current->next;

        while (current)
        {
            _fs_dump_node(current, _prefix);
            current = current->next;
        }

        if (prefix)
            kfree(_prefix);
    }
}

void fs_dump_node(struct fs_node *node)
{
    printk("\r\n=== fs dump ===\r\n");

    _fs_dump_node(node, NULL);

    printk("===============\r\n\r\n");
}