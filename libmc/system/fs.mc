enum open_mode: uint32 {
    NONE  = 0,
    DIR   = (1 << 0),
    READ  = (1 << 1),
    WRITE = (1 << 2),
    EXEC  = (1 << 3),
}

enum io_error: int64 {
    INVALID_DESCRIPTOR = -1,
    NOT_FOUND          = -2,
    NOT_A_FILE         = -3,
    NOT_A_DIR          = -4,
    NOT_PERMITTED      = -5,
}

/**
 * An open file: a node bound to a read/write position and an access mode. This
 * is the open-file-description layer (Unix `struct file`) — it owns the cursor
 * `pos`, sits between a process's fd table and fs_read/fs_write, and is what a
 * file descriptor (the per-process integer index) ultimately refers to. Its
 * lifetime is managed by a refcounted pointer<file_descriptor> in the fd table.
 *
 * @field node:  the underlying VFS node
 * @field pos:   current byte offset, advanced by file_read/file_write
 * @field attrs: access mode (FS_FILE_ATTRS_READ/WRITE/EXEC bits)
 */
struct file_descriptor {
    fd_node: struct fs_node*;
    fd_pos:  uint64;
    fd_mode: uint32;
}

/**
 * Metadata returned by file_stat / the fstat syscall.
 *
 * @field st_size: file size in bytes
 * @field st_mode: access mode of the open file (FS_FILE_ATTRS_* bits)
 */
struct file_stat {
    st_size: uint64;
    st_mode: uint32;
}
