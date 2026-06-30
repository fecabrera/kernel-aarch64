import "bswap";

/**
 * Converts a 16-bit value between host and big-endian byte order. On the
 * little-endian AArch64 host this swaps the two bytes; the conversion is its
 * own inverse, so the same call reads or writes big-endian data.
 *
 * @param x: value to convert
 *
 * @return x with host and big-endian byte order exchanged
 */
@if (TARGET_ARCH == ARCH_AARCH64) {
    @inline fn be16(x: uint16) -> uint16 { return bswap16(x); }
} @else {
    @inline fn be16(x: uint16) -> uint16 { return x; }
}

/**
 * Converts a 32-bit value between host and big-endian byte order. On the
 * little-endian AArch64 host this reverses all four bytes; the conversion is
 * its own inverse, so the same call reads or writes big-endian data.
 *
 * @param x: value to convert
 *
 * @return x with host and big-endian byte order exchanged
 */
@if (TARGET_ARCH == ARCH_AARCH64) {
    @inline fn be32(x: uint32) -> uint32 { return bswap32(x); }
} @else {
    @inline fn be32(x: uint32) -> uint32 { return x; }
}

/**
 * Converts a 64-bit value between host and big-endian byte order. On the
 * little-endian AArch64 host this reverses all eight bytes; the conversion is
 * its own inverse, so the same call reads or writes big-endian data.
 *
 * @param x: value to convert
 *
 * @return x with host and big-endian byte order exchanged
 */
@if (TARGET_ARCH == ARCH_AARCH64) {
    @inline fn be64(x: uint64) -> uint64 { return bswap64(x); }
} @else {
    @inline fn be64(x: uint64) -> uint64 { return x; }
}

/**
 * Converts a 16-bit value between host and little-endian byte order. The host
 * is already little-endian, so this is a no-op provided for symmetry with be16.
 *
 * @param x: value to convert
 *
 * @return x unchanged
 */
@if (TARGET_ARCH == ARCH_AARCH64) {
    @inline fn le16(x: uint16) -> uint16 { return x; }
} @else {
    @inline fn le16(x: uint16) -> uint16 { return bswap16(x); }
}

/**
 * Converts a 32-bit value between host and little-endian byte order. The host
 * is already little-endian, so this is a no-op provided for symmetry with be32.
 *
 * @param x: value to convert
 *
 * @return x unchanged
 */
@if (TARGET_ARCH == ARCH_AARCH64) {
    @inline fn le32(x: uint32) -> uint32 { return x; }
} @else {
    @inline fn le32(x: uint32) -> uint32 { return bswap32(x); }
}

/**
 * Converts a 64-bit value between host and little-endian byte order. The host
 * is already little-endian, so this is a no-op provided for symmetry with be64.
 *
 * @param x: value to convert
 *
 * @return x unchanged
 */
@if (TARGET_ARCH == ARCH_AARCH64) {
    @inline fn le64(x: uint64) -> uint64 { return x; }
} @else {
    @inline fn le64(x: uint64) -> uint64 { return bswap64(x); }
}