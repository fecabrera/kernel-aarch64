import "fs";
import "system/fs";

/**
 * Initializes fd as an open file on node with the given access mode, positioned
 * at offset 0.
 *
 * @param fd:    caller-allocated file_descriptor to initialize
 * @param node:  VFS node to bind
 * @param attrs: access mode (FS_FILE_ATTRS_READ/WRITE/EXEC bits)
 */
fn file_init(fd: struct file_descriptor*, node: struct fs_node*, attrs: uint32) {
    fd->node = node;
    fd->attrs = attrs;
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
        return -999;

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
        return -999;

    fd->pos = fd->pos + n as uint64;
    return n;
}

/**
 * Fills st with metadata for the open file: the node's size and access mode.
 * Thin wrapper that forwards the descriptor's node to file_node_stat.
 *
 * @param fd: open file to stat
 * @param st: caller-allocated file_stat to fill
 *
 * @return 0 on success
 */
fn file_stat(fd: struct file_descriptor*, st: struct file_stat*) -> int64 {
    return file_node_stat(fd->node, st);
}

/**
 * Fills st with metadata for a node directly (no open descriptor required):
 * st_size from node->file_size and st_mode from node->attrs. Backs both
 * file_stat (fd-based) and the path-based stat syscall.
 *
 * @param node: node to stat
 * @param st:   caller-allocated file_stat to fill
 *
 * @return 0 on success
 */
fn file_node_stat(node: struct fs_node*, st: struct file_stat*) -> int64 {
    st->st_size = node->file_size;
    st->st_mode = node->attrs;
    return 0;
}
