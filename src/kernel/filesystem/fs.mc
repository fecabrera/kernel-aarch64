struct fs_node {
    name: uint8*;
    file_size: uint64;
    attrs: uint16;
    info: uint8*;             // driver-private pointer (e.g. fat32_entry_reference *)
    mount: struct fs_mount*; // vfs_mount * for this node's covering mountpoint, or NULL
    next: struct fs_node*;
    child: struct fs_node*;
}

struct fs_mount {
    mountpoint: uint8*; // VFS path this mount is registered under
    device: uint8*;     // VFS path of the underlying block device, or NULL
    info: uint8*;       // filesystem-private superblock data (heap-allocated; freed by vfs_destroy_mountpoint)
    root: struct fs_node*;
    read: fn (struct fs_node*, uint8*, uint64, uint64) -> int32;
    write: fn (struct fs_node*, uint8*, uint64, uint64) -> int32;
}