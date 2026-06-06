#include <debug.h>
#include <dsa/deque.h>
#include <mm/heap.h>
#include "vfs.h"

struct fs_node *_fs_root;
struct deque64 _vfs_mp_table;

void vfs_init()
{
    _fs_root = fs_create_folder(NULL, 0, 0, 0);
    fs_add_subfolder(_fs_root, "volumes", 7, 0, 0);
}

int vfs_create_mountpoint(char *mountpoint, struct io_module *module, void *data)
{
    struct fs_node *mp_node = vfs_get_node(mountpoint);
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
    vfs_mp->module = module;
    vfs_mp->data = data;

    strcpy(vfs_mp->mountpoint, mountpoint);

    deque64_add_left(&_vfs_mp_table, (uintptr_t)vfs_mp);

    return 0;
}

static int _vfs_mount_mp_eq(struct deque64_entry *entry, void *mountpoint)
{
    struct vfs_mount *mount = (struct vfs_mount *)entry->value;
    return strcmp(mount->mountpoint, (char *)mountpoint);
}

int vfs_destroy_mountpoint(char *mountpoint)
{
    uint64_t value;
    if (deque64_find_remove(&_vfs_mp_table, NULL, &_vfs_mount_mp_eq, mountpoint, &value) != 0)
    {
        printk("[vfs] mountpoint \"%s\" doesn't exist!\r\n", mountpoint);
        return -1;
    }

    struct vfs_mount *vfs_mp = (struct vfs_mount *)value;

    // get parent
    struct fs_node *parent_ref = fs_get_child(vfs_mp->root, "..");
    if (parent_ref != NULL)
        fs_remove_child(parent_ref->child, vfs_mp->root->name);

    fs_destroy_node(vfs_mp->root);
    kfree(vfs_mp->mountpoint);
    kfree(vfs_mp);

    return 0;
}

struct fs_node *vfs_get_node(char *pathname)
{
    char *_pathname = pathname;
    struct fs_node *current = _fs_root;

    char str[256] = {0};
    char *_str = str;
    while (1)
    {
        if (*_pathname == NULL)
        {
            if (strlen(str) > 0)
                return fs_get_child(current, str);

            return current;
        }

        if (*_pathname == '/')
        {
            if (strlen(str) > 0)
                current = fs_get_child(current, str);

            memset(str, 0, 256);
            _str = str;
        }
        else
        {
            *_str++ = *_pathname;
        }

        _pathname++;
    }
    return NULL;
}

void vfs_dump_fs()
{
    fs_dump_node(_fs_root->child, "");
}
