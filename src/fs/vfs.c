#include "vfs.h"
#include <debug.h>
#include <dsa/hashmap.h>
#include <dsa/stack.h>
#include <mm/heap.h>

struct fs_node *_fs_root;
struct hashmap64 _vfs_mp_table;

static struct fs_node *_vfs_get_node(char *pathname, struct fs_node *root) {
    char *_pathname = pathname;
    struct fs_node *current = root;

    char str[256] = {0};
    char *_str = str;

    while (current) {
        // end of string
        if (*_pathname == '\0') {
            if (strlen(str) > 0)
                // pathname is of type "/abc/def", `current` holds "/abc" so we
                // need to find "def"
                return fs_get_child(current, str);

            // pathname is of type "/abc/def/" which needs to be resolved to
            // "/abc/def" `current` already holds "/abc/def" so return it
            return current;
        }

        // dir separator
        if (*_pathname == '/') {
            if (strlen(str) > 0)
                current = fs_get_child(current, str);

            // reset buffer
            memset(str, 0, 256);
            _str = str;
        } else {
            // store next character
            *_str++ = *_pathname;
        }

        _pathname++;
    }

    // if current == NULL the node doesn't exist
    return NULL;
}

void vfs_init() {
    // initialize table
    hashmap64_init(&_vfs_mp_table, 10);

    // create root and volumes
    _fs_root = fs_create_folder(NULL, 0, NULL, NULL);
    fs_add_subfolder(_fs_root, "volumes", 0, NULL, NULL);
}

struct vfs_mount *vfs_create_mountpoint(char *mountpoint, char *device, void *info,
                                        vfs_handler_t read, vfs_handler_t write) {
    struct fs_node *mp_node = _vfs_get_node(mountpoint, _fs_root);
    if (mp_node == NULL) {
        dprintk("[vfs] mountpoint \"%s\" not found!\r\n", mountpoint);
        return NULL;
    }

    if ((mp_node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
        dprintk("[vfs] \"%s\" is not a folder!\r\n", mountpoint);
        return NULL;
    }

    struct vfs_mount *vfs_mp = (struct vfs_mount *)kmalloc(sizeof(struct vfs_mount));

    vfs_mp->mountpoint = (char *)kmalloc(strlen(mountpoint) + 1);
    strcpy(vfs_mp->mountpoint, mountpoint);

    vfs_mp->info = info;
    vfs_mp->root = mp_node;
    vfs_mp->read = read;
    vfs_mp->write = write;

    if (device) {
        vfs_mp->device = (char *)kmalloc(strlen(device) + 1);
        strcpy(vfs_mp->device, device);
    } else {
        vfs_mp->device = NULL;
    }

    hashmap64_set(&_vfs_mp_table, mountpoint, (uintptr_t)vfs_mp);

    dprintk("[vfs] \"%s\" mounted successfully\r\n", mountpoint);

    return vfs_mp;
}

struct vfs_mount *vfs_get_mountpoint(char *mountpoint) {
    uint64_t value;
    if (hashmap64_get(&_vfs_mp_table, mountpoint, &value) == 0)
        return NULL;

    return (struct vfs_mount *)value;
}

struct fs_node *vfs_get_node_for_path(char *pathname) { return _vfs_get_node(pathname, _fs_root); }

int vfs_destroy_mountpoint(char *mountpoint) {
    struct vfs_mount *mp = vfs_get_mountpoint(mountpoint);
    if (mp == NULL) {
        dprintk("[vfs] mountpoint \"%s\" doesn't exist!\r\n", mountpoint);
        return -1;
    }

    // get parent
    struct fs_node *parent_ref = fs_get_child(mp->root, "..");
    if (parent_ref != NULL)
        fs_remove_child(parent_ref->child, mp->root->name);

    fs_destroy_node(mp->root);
    kfree(mp->mountpoint);
    if (mp->device)
        kfree(mp->device);
    if (mp->info)
        kfree(mp->info);
    kfree(mp);

    dprintk("[vfs] \"%s\" unmounted successfully\r\n", mountpoint);

    return 0;
}

size_t vfs_get_file_size(char *pathname) {
    struct fs_node *node = _vfs_get_node(pathname, _fs_root);

    if (node == NULL)
        return 0;

    return fs_get_node_file_size(node);
}

int vfs_read(char *pathname, uint8_t *buffer, size_t count, size_t offset) {
    dprintk("[vfs] read(): file=\"%s\", buff=0x%08x, count=%d, offset=%d\r\n", pathname, buffer,
            count, offset);

    struct fs_node *node = _vfs_get_node(pathname, _fs_root);
    if (node == NULL) {
        dprintk("[vfs] file \"%s\" not found!\r\n", pathname);
        return VFS_IO_ERROR_FILE_NOT_FOUND;
    }

    struct vfs_mount *mp = node->mount;
    if (mp == NULL) {
        dprintk("[vfs] mountpoint for \"%s\" not found!\r\n", pathname);
        return VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND;
    }

    if (mp->read == NULL) {
        dprintk("[vfs] mountpoint \"%s\" didn't provide a `read` handler!\r\n", mp->mountpoint);
        return VFS_IO_ERROR_HANDLER_NOT_PROVIDED;
    }

    return mp->read(node, buffer, count, offset);
}

int vfs_write(char *pathname, uint8_t *buffer, size_t count, size_t offset) {
    dprintk("[vfs] write(): file=\"%s\", buff=0x%08x, count=%d, offset=%d\r\n", pathname, buffer,
            count, offset);

    struct fs_node *node = _vfs_get_node(pathname, _fs_root);
    if (node == NULL) {
        dprintk("[vfs] file \"%s\" not found!\r\n", pathname);
        return VFS_IO_ERROR_FILE_NOT_FOUND;
    }

    struct vfs_mount *mp = node->mount;
    if (mp == NULL) {
        dprintk("[vfs] mountpoint for file \"%s\" not found!\r\n", pathname);
        return VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND;
    }

    if (mp->write == NULL) {
        dprintk("[vfs] mountpoint \"%s\" didn't provide a `write` handler!\r\n", mp->mountpoint);
        return VFS_IO_ERROR_HANDLER_NOT_PROVIDED;
    }

    return mp->write(node, buffer, count, offset);
}

struct fs_node *vfs_create_dir(char *path, char *name, uint16_t attrs, void *mount) {
    struct fs_node *parent = _vfs_get_node(path, _fs_root);
    struct fs_node *node = fs_add_subfolder(parent, name, attrs, NULL, mount);
    if (node == NULL) {
        dprintk("[vfs] cannot create \"%s/%s\"!\r\n", path, name);
        return NULL;
    }

    return node;
}

struct fs_node *vfs_create_file(char *path, char *name, size_t file_size, uint16_t attrs,
                                void *mount) {
    struct fs_node *parent = _vfs_get_node(path, _fs_root);
    struct fs_node *node = fs_add_file_to_folder(parent, name, file_size, attrs, NULL, mount);
    if (node == NULL) {
        dprintk("[vfs] cannot create \"%s/%s\"!\r\n", path, name);
        return NULL;
    }

    return node;
}

void vfs_dump_fs() { fs_dump_node(_fs_root->child, ""); }
