import "fs";

const FS_FILE_STATUS_OPEN = 0;
const FS_FILE_STATUS_CLOSED = 1;

const FS_FILE_ATTRS_NONE = 0;
const FS_FILE_ATTRS_READ = (1 << 0);
const FS_FILE_ATTRS_WRITE = (1 << 1);
const FS_FILE_ATTRS_EXEC = (1 << 2);

const FILE_IO_ERROR_NOT_PERMITTED = -1;

/**
 * An open file: a node bound to a read/write position and an access mode. This
 * is the open-file-description layer (Unix `struct file`) — it owns the cursor
 * `pos`, sits between a process's fd table and fs_read/fs_write, and is what a
 * file descriptor (the per-process integer index) ultimately refers to.
 *
 * @field node:   the underlying VFS node, or null once closed
 * @field status: FS_FILE_STATUS_OPEN or FS_FILE_STATUS_CLOSED
 * @field attrs:  access mode (FS_FILE_ATTRS_READ/WRITE/EXEC bits)
 * @field pos:    current byte offset, advanced by file_read/file_write
 */
struct file_descriptor {
    node: struct fs_node*;
    status: uint32;
    attrs: uint32;
    pos: uint64;
}

/**
 * Initializes fd as an open file on node with the given access mode, positioned
 * at offset 0.
 *
 * @param fd:    caller-allocated file_descriptor to initialize
 * @param node:  VFS node to bind
 * @param attrs: access mode (FS_FILE_ATTRS_READ/WRITE/EXEC bits)
 */
fn file_open(fd: struct file_descriptor*, node: struct fs_node*, attrs: uint32) {
    fd->node = node;
    fd->status = FS_FILE_STATUS_OPEN;
    fd->attrs = attrs;
    fd->pos = 0;
}

/**
 * Marks fd closed: clears its node, resets the position, and drops all access
 * bits so subsequent file_read/file_write return FILE_IO_ERROR_NOT_PERMITTED.
 *
 * @param fd: file_descriptor to close
 */
fn file_close(fd: struct file_descriptor*) {
    fd->node = null;
    fd->status = FS_FILE_STATUS_CLOSED;
    fd->attrs = FS_FILE_ATTRS_NONE;
    fd->pos = 0;
}

/**
 * Reads up to count bytes from fd into buffer at the current position, via
 * fs_read, then advances fd->pos by the number of bytes read.
 *
 * @param fd:     open file to read from
 * @param buffer: output buffer
 * @param count:  maximum number of bytes to read
 *
 * @return number of bytes read on success, FILE_IO_ERROR_NOT_PERMITTED if the
 *         file is not readable, or a negative error on fs_read failure
 */
fn file_read(fd: struct file_descriptor*, buffer: uint8*, count: uint64) -> int64 {
    if((fd->attrs & FS_FILE_ATTRS_READ) == 0)
        return FILE_IO_ERROR_NOT_PERMITTED;

    let n = fs_read(fd->node, buffer, count, fd->pos);
    if(n < 0)
        // @todo: parse response and translate to FILE_IO_ERROR_*
        return -2;

    fd->pos = fd->pos + n as uint64;
    return n;
}

/**
 * Writes up to count bytes from buffer to fd at the current position, via
 * fs_write, then advances fd->pos by the number of bytes written.
 *
 * @param fd:     open file to write to
 * @param buffer: input buffer
 * @param count:  number of bytes to write
 *
 * @return number of bytes written on success, FILE_IO_ERROR_NOT_PERMITTED if the
 *         file is not writable, or a negative error on fs_write failure
 */
fn file_write(fd: struct file_descriptor*, buffer: uint8*, count: uint64) -> int64 {
    if((fd->attrs & FS_FILE_ATTRS_WRITE) == 0)
        return FILE_IO_ERROR_NOT_PERMITTED;

    let n = fs_write(fd->node, buffer, count, fd->pos);
    if(n < 0)
        // @todo: parse response and translate to FILE_IO_ERROR_*
        return -2;

    fd->pos = fd->pos + n as uint64;
    return n;
}
