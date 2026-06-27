enum open_mode: uint32 {
    NONE  = 0,
    DIR   = (1 << 0),
    READ  = (1 << 1),
    WRITE = (1 << 2),
    EXEC  = (1 << 3),
}

enum node_attrs: uint32 {
    DIR              = (0 << 0),
    FILE             = (1 << 0),
    // RESERVED         = (1 << 1),
    LINK             = (1 << 2),
    HIDDEN           = (1 << 3),
    READ             = (1 << 4),
    WRITE            = (1 << 5),
    EXECUTE          = (1 << 6),
    TYPE_MASK        = (node_attrs::FILE),
    PERMISSIONS_MASK = (node_attrs::READ | node_attrs::WRITE | node_attrs::EXECUTE),
    FLAG_MASK        = (node_attrs::LINK | node_attrs::HIDDEN),
}

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
    d_name: uint8*;
}

