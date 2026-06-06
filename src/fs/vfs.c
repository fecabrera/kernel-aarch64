#include <debug.h>
#include "vfs.h"

struct fs_node *_fs_root;

void vfs_init()
{
    _fs_root = fs_create_folder(NULL, 0, 0);
    fs_add_subfolder(_fs_root, "volumes", 7, 0);
}

int vfs_mount(char *mountpoint, struct fs_node *node)
{
    struct fs_node *mp_node = vfs_get_node(mountpoint);
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

int vfs_unmount(char *mountpoint)
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

struct fs_node *vfs_get_node(char *pathname)
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

        _pathname++;
    }
    return NULL;
}

void vfs_dump_fs()
{
    fs_dump_node(_fs_root->child, "");
}
