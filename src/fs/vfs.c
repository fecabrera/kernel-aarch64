#include <debug.h>
#include <dsa/hashmap.h>
#include <dsa/stack.h>
#include <mm/heap.h>
#include "vfs.h"

struct fs_node *_fs_root;
struct hashmap64 _vfs_mp_table;

static struct fs_node *_vfs_get_node(char *pathname, struct fs_node *root)
{
    char *_pathname = pathname;
    struct fs_node *current = root;

    char str[256] = {0};
    char *_str = str;

    while (current)
    {
        // end of string
        if (*_pathname == '\0')
        {
            if (strlen(str) > 0)
                // pathname is of type "/abc/def", `current` holds "/abc" so we need to find "def"
                return fs_get_child(current, str);

            // pathname is of type "/abc/def/" which needs to be resolved to "/abc/def"
            // `current` already holds "/abc/def" so return it
            return current;
        }

        // dir separator
        if (*_pathname == '/')
        {
            if (strlen(str) > 0)
                current = fs_get_child(current, str);

            // reset buffer
            memset(str, 0, 256);
            _str = str;
        }
        else
        {
            // store next character
            *_str++ = *_pathname;
        }

        _pathname++;
    }

    // if current == NULL the node doesn't exist
    return NULL;
}

void vfs_init()
{
    // initialize table
    hashmap64_init(&_vfs_mp_table, 10);

    // create root and volumes
    _fs_root = fs_create_folder(NULL, 0, 0);
    fs_add_subfolder(_fs_root, "volumes", 0, 0);
}

int vfs_create_mountpoint(char *mountpoint, vfs_handler_t read, vfs_handler_t write, void *data)
{
    struct fs_node *mp_node = _vfs_get_node(mountpoint, _fs_root);
    if (mp_node == NULL)
    {
        printk("[vfs] mountpoint \"%s\" not found!\r\n", mountpoint);
        return -1;
    }

    if ((mp_node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER)
    {
        printk("[vfs] \"%s\" is not a folder!\r\n", mountpoint);
        return -2;
    }

    struct vfs_mount *vfs_mp = (struct vfs_mount *)kmalloc(sizeof(struct vfs_mount));
    vfs_mp->mountpoint = (char *)kmalloc(strlen(mountpoint) + 1);
    vfs_mp->root = mp_node;
    vfs_mp->read = read;
    vfs_mp->write = write;
    vfs_mp->data = data;

    strcpy(vfs_mp->mountpoint, mountpoint);

    hashmap64_set(&_vfs_mp_table, mountpoint, (uintptr_t)vfs_mp);

    printk("[vfs] \"%s\" mounted successfully\r\n", mountpoint);

    return 0;
}

struct vfs_mount *vfs_get_mountpoint(char *mountpoint)
{
    uint64_t value;
    if (hashmap64_get(&_vfs_mp_table, mountpoint, &value) == 0)
        return NULL;

    return (struct vfs_mount *)value;
}

struct vfs_mount *vfs_get_mountpoint_for_path(char *pathname)
{
    struct vfs_mount *mp = NULL;
    char str[256] = {0};
    char *_str = str;
    char *_pathname = pathname;

    while (*_pathname)
    {
        if (*_pathname == '/')
        {
            uint64_t value;
            if (hashmap64_get(&_vfs_mp_table, str, &value) == 1)
                mp = (struct vfs_mount *)value;
        }

        *_str++ = *_pathname++;
    }

    return mp;
}

int vfs_destroy_mountpoint(char *mountpoint)
{
    struct vfs_mount *mp = vfs_get_mountpoint(mountpoint);
    if (mp == NULL)
    {
        printk("[vfs] mountpoint \"%s\" doesn't exist!\r\n", mountpoint);
        return -1;
    }

    // get parent
    struct fs_node *parent_ref = fs_get_child(mp->root, "..");
    if (parent_ref != NULL)
        fs_remove_child(parent_ref->child, mp->root->name);

    fs_destroy_node(mp->root);
    kfree(mp->mountpoint);
    kfree(mp);

    printk("[vfs] \"%s\" unmounted successfully\r\n", mountpoint);

    return 0;
}

int vfs_read(char *pathname, uint8_t *buffer, size_t n)
{
    struct fs_node *node = _vfs_get_node(pathname, _fs_root);
    if (node == NULL)
    {
        printk("[vfs] file \"%s\" not found!\r\n", pathname);
        return VFS_IO_ERROR_FILE_NOT_FOUND;
    }

    struct vfs_mount *mp = vfs_get_mountpoint_for_path(pathname);
    if (mp == NULL)
    {
        printk("[vfs] mountpoint for \"%s\" not found!\r\n", pathname);
        return VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND;
    }

    if (mp->read == NULL)
    {
        printk("[vfs] mountpoint \"%s\" didn't provide a `read` handler!\r\n", mp->mountpoint);
        return VFS_IO_ERROR_HANDLER_NOT_PROVIDED;
    }

    return mp->read(node, buffer, n);
}

int vfs_write(char *pathname, uint8_t *buffer, size_t n)
{
    struct fs_node *node = _vfs_get_node(pathname, _fs_root);
    if (node == NULL)
    {
        printk("[vfs] file \"%s\" not found!\r\n", pathname);
        return VFS_IO_ERROR_FILE_NOT_FOUND;
    }

    struct vfs_mount *mp = vfs_get_mountpoint_for_path(pathname);
    if (mp == NULL)
    {
        printk("[vfs] mountpoint for file \"%s\" not found!\r\n", pathname);
        return VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND;
    }

    if (mp->write == NULL)
    {
        printk("[vfs] mountpoint \"%s\" didn't provide a `write` handler!\r\n", mp->mountpoint);
        return VFS_IO_ERROR_HANDLER_NOT_PROVIDED;
    }

    return mp->write(node, buffer, n);
}

struct fs_node *vfs_create_dir(char *path, char *name, uint16_t attrs)
{
    struct fs_node *parent = _vfs_get_node(path, _fs_root);
    struct fs_node *node = fs_add_subfolder(parent, name, attrs, 0);
    if (node == NULL)
    {
        printk("[vfs] cannot create \"%s/%s\"!\r\n", path, name);
        return NULL;
    }

    return node;
}

void vfs_dump_fs()
{
    fs_dump_node(_fs_root->child, "");
}
