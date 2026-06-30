/**
 * Reverses the byte order of a 16-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its two bytes reversed
 */
@inline @asm fn bswap16(value: uint16) -> uint16 {
    "rev16 $out, $0"
}

/**
 * Reverses the byte order of a 32-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its four bytes reversed
 */
@inline @asm fn bswap32(value: uint32) -> uint32 {
    "rev ${out:w}, ${0:w}"
}

/**
 * Reverses the byte order of a 64-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its eight bytes reversed
 */
@inline @asm fn bswap64(value: uint64) -> uint64 {
    "rev ${out:x}, ${0:x}"
}
