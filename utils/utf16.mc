/**
 * Copies up to n little-endian UTF-16 code units from src into dst as ASCII
 * bytes, applying _le16 byte-swap before conversion. Stops at null terminator.
 * Always null-terminates dst. Non-ASCII code points become '?'.
 *
 * @param dst: output buffer (must hold at least n+1 bytes)
 * @param src: UTF-16LE source array
 * @param n:   maximum number of code units to convert
 *
 * @return dst
 */
@extern fn utf16lencpy(dst: char*, src: uint16*, n: uint64) -> char*;

/**
 * Copies up to n big-endian UTF-16 code units from src into dst as ASCII
 * bytes, applying _be16 byte-swap before conversion. Stops at null terminator.
 * Always null-terminates dst. Non-ASCII code points become '?'.
 *
 * @param dst: output buffer (must hold at least n+1 bytes)
 * @param src: UTF-16BE source array
 * @param n:   maximum number of code units to convert
 *
 * @return dst
 */
@extern fn utf16bencpy(dst: char*, src: uint16*, n: uint64) -> char*;
