const FS_FILE_ATTRS_NONE = 0;
const FS_FILE_ATTRS_READ = (1 << 0);
const FS_FILE_ATTRS_WRITE = (1 << 1);
const FS_FILE_ATTRS_EXEC = (1 << 2);

const FILE_IO_ERROR_INVALID_DESCRIPTOR = -1;
const FILE_IO_ERROR_NOT_FOUND = -2;
const FILE_IO_ERROR_NOT_A_FILE = -3;
const FILE_IO_ERROR_NOT_PERMITTED = -4;

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
    node: struct fs_node*;
    pos: uint64;
    attrs: uint32;
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