/**
 * Access modes requested when opening a node, passed as a bitmask to the open
 * syscall. NONE opens with no particular intent; the remaining flags may be
 * OR'd together.
 *
 * @field NONE:  no access flags
 * @field DIR:   open the node as a directory
 * @field READ:  request read access
 * @field WRITE: request write access
 * @field EXEC:  request execute access
 */
enum open_mode: uint32 {
    NONE  = 0,
    DIR   = (1 << 0),
    READ  = (1 << 1),
    WRITE = (1 << 2),
    EXEC  = (1 << 3),
}

/**
 * Attributes describing a filesystem node, stored per-node and reported through
 * stat. The low bits encode the node type, higher bits encode flags and access
 * permissions. Use the *_MASK values to isolate each group of bits.
 *
 * @field DIR:              node is a directory (type bit clear)
 * @field FILE:             node is a regular file (type bit set)
 * @field LINK:             node is a symbolic link
 * @field HIDDEN:           node is hidden
 * @field READ:             node is readable
 * @field WRITE:            node is writable
 * @field EXECUTE:          node is executable
 * @field TYPE_MASK:        mask selecting the type bits
 * @field FLAG_MASK:        mask selecting the flag bits (LINK, HIDDEN)
 * @field PERMISSIONS_MASK: mask selecting the permission bits (READ, WRITE, EXECUTE)
 */
enum node_attrs: uint32 {
    DIR              = 0,
    FILE             = 1,
    // RESERVED         = (1 << 1),
    LINK             = (1 << 2),
    HIDDEN           = (1 << 3),
    READ             = (1 << 4),
    WRITE            = (1 << 5),
    EXECUTE          = (1 << 6),
    TYPE_MASK        = (node_attrs::FILE),
    FLAG_MASK        = (node_attrs::LINK | node_attrs::HIDDEN),
    PERMISSIONS_MASK = (node_attrs::READ | node_attrs::WRITE | node_attrs::EXECUTE),
}

/**
 * Negative error codes returned by filesystem operations. A non-negative return
 * value indicates success; a negative value matches one of these variants.
 *
 * @field INVALID_DESCRIPTOR: the given file descriptor is not valid
 * @field NOT_FOUND:          the requested node does not exist
 * @field NOT_A_FILE:         the target is not a regular file
 * @field NOT_A_DIR:          the target is not a directory
 * @field NOT_PERMITTED:      the operation is not permitted for this node
 */
enum io_error: int64 {
    INVALID_DESCRIPTOR = -1,
    NOT_FOUND          = -2,
    NOT_A_FILE         = -3,
    NOT_A_DIR          = -4,
    NOT_PERMITTED      = -5,
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

/**
 * The kind of filesystem object a directory entry refers to, reported in
 * dirent.d_type by the getdents syscall.
 */
enum dent_type: uint16 {
    DIRECTORY   = 0,
    REGULAR     = 1,
    BLK_DEVICE  = 2,
    CHAR_DEVICE = 3,
}

/**
 * A single directory entry returned by the getdents syscall. Entries are
 * variable-length: d_size is the total byte size of this record (header +
 * name), so the next entry begins d_size bytes after the start of this one.
 *
 * @field d_size: total size of this entry in bytes, for walking to the next one
 * @field d_type: kind of object this entry names (see dent_type)
 * @field d_name: null-terminated entry name
 */
struct dirent {
    d_size: uint16;
    d_type: dent_type;
    d_name: byte[];
}
