#include "vfs.h"
#include <debug.h>
#include <dsa/hashmap.h>
#include <dsa/stack.h>
#include <mm/heap.h>

// struct fs_node *_fs_root;
// struct hashmap64 _vfs_mp_table;

// void _vfs_init() {
//     // initialize table
//     hashmap64_init(&_vfs_mp_table, 10);
// }

// struct vfs_mount *_vfs_create_mountpoint(char *mountpoint, char *device, void *info,
//                                          vfs_handler_t read, vfs_handler_t write) {
//     struct fs_node *mp_node = vfs_get_node_for_path(mountpoint, NULL);
//     if (mp_node == NULL) {
//         dprintk("[vfs] mountpoint \"%s\" not found!\r\n", mountpoint);
//         return NULL;
//     }

//     if ((mp_node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
//         dprintk("[vfs] \"%s\" is not a folder!\r\n", mountpoint);
//         return NULL;
//     }

//     struct vfs_mount *vfs_mp = (struct vfs_mount *)kmalloc(sizeof(struct vfs_mount));

//     vfs_mp->mountpoint = (char *)kmalloc(strlen(mountpoint) + 1);
//     strcpy(vfs_mp->mountpoint, mountpoint);

//     vfs_mp->info = info;
//     vfs_mp->root = mp_node;
//     vfs_mp->read = read;
//     vfs_mp->write = write;

//     if (device) {
//         vfs_mp->device = (char *)kmalloc(strlen(device) + 1);
//         strcpy(vfs_mp->device, device);
//     } else {
//         vfs_mp->device = NULL;
//     }

//     hashmap64_set(&_vfs_mp_table, mountpoint, (uintptr_t)vfs_mp);

//     dprintk("[vfs] \"%s\" mounted successfully\r\n", mountpoint);

//     return vfs_mp;
// }

// struct vfs_mount *vfs_get_mountpoint(char *mountpoint) {
// uint64_t value;
// if (hashmap64_get(&_vfs_mp_table, mountpoint, &value) == 0)
//     return NULL;

// return (struct vfs_mount *)value;
// }

// int vfs_destroy_mountpoint(char *mountpoint) {
//     struct vfs_mount *mp = vfs_get_mountpoint(mountpoint);
//     if (mp == NULL) {
//         dprintk("[vfs] mountpoint \"%s\" doesn't exist!\r\n", mountpoint);
//         return -1;
//     }

//     // get parent
//     struct fs_node *parent_ref = fs_get_child(mp->root, "..");
//     if (parent_ref != NULL)
//         fs_remove_child(parent_ref->child, mp->root->name);

//     fs_destroy_node(mp->root);
//     kfree(mp->mountpoint);
//     if (mp->device)
//         kfree(mp->device);
//     if (mp->info)
//         kfree(mp->info);
//     kfree(mp);

//     dprintk("[vfs] \"%s\" unmounted successfully\r\n", mountpoint);

//     return 0;
// }
