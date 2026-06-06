#include <debug.h>
#include <stdlib.h>
#include <mm/heap.h>
#include <dsa/hashmap.h>
#include <arch/cpu.h>
#include "filesystem.h"

struct fs_node *_fs_root;

void fs_init()
{
    _fs_root = fs_create_folder(NULL, 0, 0);
    fs_add_subfolder(_fs_root, "volumes", 7, 0);
}

int fs_mount(char *mountpoint, struct fs_node *node)
{
    struct fs_node *mp_node = fs_get_node(mountpoint);
    if (mp_node == NULL)
    {
        printk("[fs] mountpoint \"%s\" not found!", mountpoint);
        return -1;
    }

    if ((mp_node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER)
    {
        printk("[fs] \"%s\" is not a folder!", mountpoint);
        return -1;
    }

    fs_add_to_folder(mp_node, node);

    return 0;
}

int fs_unmount(char *mountpoint)
{
    // uintptr_t mount_ptr;
    // if (!hashmap64_get(&_mounts, mountpoint, &mount_ptr))
    // {
    //     printk("[fs] mountpoint \"%s\" doesn't exist!", mountpoint);
    //     return -1;
    // }

    // hashmap64_remove(&_mounts, mountpoint);
    // fs_destroy_node((struct fs_node *)mount_ptr);
    return 0;
}

struct fs_node *fs_get_children(struct fs_node *node, char *name)
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

struct fs_node *fs_get_node(char *pathname)
{
    printk("[fs] searching node \"%s\"\r\n", pathname);
    char *_pathname = pathname;
    struct fs_node *current = _fs_root;

    char str[256] = {0};
    char *_str = str;
    while (1)
    {
        if (*_pathname == NULL)
        {
            if (strlen(str) > 0)
                return fs_get_children(current, str);

            return current;
        }

        if (*_pathname == '/')
        {
            if (strlen(str) > 0)
            {
                printk("[fs] searching node \"%s\" in \"%s\"\r\n", str, current->name || "<root>");
                current = fs_get_children(current, str);

                if (current)
                    printk("[fs] node \"%s\" found\r\n", current->name);
            }

            memset(str, 0, 256);
            _str = str;
        }
        else
        {
            *_str++ = *_pathname;
        }

        *_pathname++;
    }
    return NULL;
}

struct fs_node *fs_create_node(char *name, size_t name_size, uint16_t attrs, struct fs_node *next, struct fs_node *child)
{
    struct fs_node *node = (struct fs_node *)kmalloc(sizeof(struct fs_node));

    node->name = NULL;
    node->attrs = attrs;
    node->next = next;
    node->child = child;

    if (name)
        fs_node_rename(node, name, name_size);

    return node;
}

void fs_destroy_node(struct fs_node *node)
{
    if (node->child)
        fs_destroy_node(node->child);

    if (node->next)
        fs_destroy_node(node->next);

    kfree(node->name);
    kfree(node);
}

static struct fs_node *_fs_create_self_ref(struct fs_node *folder)
{
    struct fs_node *self_ref = fs_create_node(".", 1, FS_NODE_ATTRS_TYPE_FOLDER, NULL, NULL);
    self_ref->child = folder;
    fs_add_to_folder(folder, self_ref);
    return self_ref;
}

static struct fs_node *_fs_create_parent_ref(struct fs_node *parent, struct fs_node *folder)
{
    struct fs_node *parent_ref = fs_create_node("..", 2, FS_NODE_ATTRS_TYPE_FOLDER, NULL, NULL);
    parent_ref->child = parent;
    fs_add_to_folder(folder, parent_ref);
    return parent_ref;
}

struct fs_node *fs_create_file(char *name, size_t name_size, uint16_t attrs)
{
    size_t _name_size = strnlen(name, name_size);
    uint16_t _attrs = (attrs & FS_NODE_ATTRS_FLAG_MASK) | FS_NODE_ATTRS_TYPE_FILE;

    return fs_create_node(name, _name_size, _attrs, NULL, NULL);
}

struct fs_node *fs_create_folder(char *name, size_t name_size, uint16_t attrs)
{
    size_t _name_size = 0;
    if (name)
        _name_size = strnlen(name, name_size);

    uint16_t _attrs = (attrs & FS_NODE_ATTRS_FLAG_MASK) | FS_NODE_ATTRS_TYPE_FOLDER;

    struct fs_node *folder = fs_create_node(name, _name_size, _attrs, NULL, NULL);
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

struct fs_node *fs_add_file_to_folder(struct fs_node *parent, char *name, size_t name_size, uint16_t attrs)
{
    if ((parent->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER)
    {
        dprintk("[filesystem] Node is not a folder!\r\n");
        return NULL;
    }

    struct fs_node *file = fs_create_file(name, name_size, attrs);
    fs_add_to_folder(parent, file);
    return file;
}

struct fs_node *fs_add_subfolder(struct fs_node *parent, char *name, size_t name_size, uint16_t attrs)
{
    if ((parent->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER)
    {
        dprintk("[filesystem] Node is not a folder!\r\n");
        return NULL;
    }

    struct fs_node *folder = fs_create_folder(name, name_size, attrs);
    _fs_create_parent_ref(parent, folder);
    fs_add_to_folder(parent, folder);
    return folder;
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

static void _fs_dump_file(struct fs_node *node, char *prefix)
{
    if (node->name == NULL)
        return;

    if (prefix != NULL)
        printk("%s/", prefix);

    printk("%s\r\n", node->name);
}

static void _fs_dump_node(struct fs_node *node, char *prefix)
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
            _fs_dump_node(node->child, _prefix);

        if (prefix && node->name)
            kfree(_prefix);
    }

    if (node->next)
        _fs_dump_node(node->next, prefix);
}

void fs_dump_node(struct fs_node *node)
{
    printk("\r\n=== fs dump ===\r\n");

    _fs_dump_node(node->child, node->name);

    printk("===============\r\n\r\n");
}

void fs_dump_fs()
{
    _fs_dump_node(_fs_root->child, "");
}