#include <debug.h>
#include <stdlib.h>
#include <mm/heap.h>
#include <dsa/hashmap.h>
#include <arch/cpu.h>
#include "filesystem.h"

struct fs_node *fs_get_child(struct fs_node *node, char *name)
{
    struct fs_node *current = node->child;
    while (current)
    {
        if (strcmp(current->name, name) == 0)
            return current;

        current = current->next;
    }
    return NULL;
}

int fs_remove_child(struct fs_node *node, char *name)
{
    struct fs_node *prev = NULL;
    struct fs_node *current = node->child;
    while (current)
    {
        if (strcmp(current->name, name) == 0)
        {
            if (prev)
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

struct fs_node *fs_create_node(char *name, uint16_t attrs, void *info, struct fs_node *next, struct fs_node *child)
{
    struct fs_node *node = (struct fs_node *)kmalloc(sizeof(struct fs_node));

    node->name = NULL;
    node->attrs = attrs;
    node->info = info;
    node->next = next;
    node->child = child;

    if (name)
        fs_node_rename(node, name);

    return node;
}

void fs_destroy_node(struct fs_node *node)
{
    if (node == NULL)
        return;

    if (strcmp(node->name, ".") != 0 && strcmp(node->name, "..") != 0 && node->child)
        fs_destroy_node(node->child);

    if (node->next)
        fs_destroy_node(node->next);

    kfree(node->name);
    kfree(node);
}

static struct fs_node *_fs_create_self_ref(struct fs_node *folder)
{
    struct fs_node *self_ref = fs_create_node(".", FS_NODE_ATTRS_TYPE_FOLDER, NULL, NULL, NULL);
    self_ref->child = folder;
    fs_add_to_folder(folder, self_ref);
    return self_ref;
}

static struct fs_node *_fs_create_parent_ref(struct fs_node *parent, struct fs_node *folder)
{
    struct fs_node *parent_ref = fs_create_node("..", FS_NODE_ATTRS_TYPE_FOLDER, NULL, NULL, NULL);
    parent_ref->child = parent;
    fs_add_to_folder(folder, parent_ref);
    return parent_ref;
}

struct fs_node *fs_create_file(char *name, uint16_t attrs, void *data)
{
    uint16_t _attrs = (attrs & FS_NODE_ATTRS_FLAG_MASK) | FS_NODE_ATTRS_TYPE_FILE;
    return fs_create_node(name, _attrs, data, NULL, NULL);
}

struct fs_node *fs_create_folder(char *name, uint16_t attrs, void *data)
{
    uint16_t _attrs = (attrs & FS_NODE_ATTRS_FLAG_MASK) | FS_NODE_ATTRS_TYPE_FOLDER;

    struct fs_node *folder = fs_create_node(name, _attrs, data, NULL, NULL);
    _fs_create_self_ref(folder);

    return folder;
}

void fs_add_to_folder(struct fs_node *parent, struct fs_node *node)
{
    struct fs_node *current = parent->child;
    if (current)
    {
        while (current->next)
            current = current->next;

        current->next = node;
    }
    else
        parent->child = node;
}

struct fs_node *fs_add_file_to_folder(struct fs_node *parent, char *name, uint16_t attrs, void *data)
{
    if ((parent->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER)
    {
        dprintk("[filesystem] Node is not a folder!\r\n");
        return NULL;
    }

    struct fs_node *file = fs_create_file(name, attrs, data);
    fs_add_to_folder(parent, file);
    return file;
}

struct fs_node *fs_add_subfolder(struct fs_node *parent, char *name, uint16_t attrs, void *data)
{
    if ((parent->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER)
    {
        dprintk("[filesystem] node is not a folder!\r\n");
        return NULL;
    }

    struct fs_node *folder = fs_create_folder(name, attrs, data);
    _fs_create_parent_ref(parent, folder);
    fs_add_to_folder(parent, folder);
    return folder;
}

void fs_node_rename(struct fs_node *node, char *name)
{
    if (node->name)
        kfree(node->name);

    size_t _name_size = strlen(name);
    char *_name = (char *)kmalloc(_name_size + 1);
    strncpy(_name, name, _name_size);
    _name[_name_size] = '\0';

    node->name = _name;
}

static void _fs_dump_file(struct fs_node *node, char *prefix)
{
    if (node->name == NULL)
        return;

    if (prefix != NULL)
        printk("%s/", prefix);

    printk("%s\r\n", node->name);
}

void fs_dump_node(struct fs_node *node, char *prefix)
{
    _fs_dump_file(node, prefix);

    uint16_t attrs = node->attrs & FS_NODE_ATTRS_TYPE_MASK;
    if (attrs == FS_NODE_ATTRS_TYPE_FOLDER && strcmp(node->name, ".") != 0 && strcmp(node->name, "..") != 0)
    {
        char *_prefix;
        if (node->name && prefix)
        {
            size_t _strlen = strlen(prefix) + strlen(node->name) + 2;
            _prefix = (char *)kmalloc(_strlen);
            sprintf(_prefix, "%s/%s", prefix, node->name);
        }
        else if (prefix)
        {
            _prefix = prefix;
        }
        else
        {
            _prefix = node->name;
        }

        if (node->child)
            fs_dump_node(node->child, _prefix);

        if (prefix && node->name)
            kfree(_prefix);
    }

    if (node->next)
        fs_dump_node(node->next, prefix);
}