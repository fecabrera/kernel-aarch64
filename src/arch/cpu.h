#pragma once

#include <stdint.h>

/**
 * Reverses the byte order of a 16-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its two bytes reversed
 */
uint16_t bswap16(uint16_t x);

/**
 * Reverses the byte order of a 32-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its four bytes reversed
 */
uint32_t bswap32(uint32_t x);

/**
 * Reverses the byte order of a 64-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its eight bytes reversed
 */
uint64_t bswap64(uint64_t x);

// Endian conversion macros. On a little-endian host (AArch64 in LE mode),
// _be32/_be64 byte-swap to/from big-endian (e.g. DTB fields);
// _le32/_le64 are no-ops. Reversed on a big-endian host.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define _be16(x) bswap16(x)
#define _be32(x) bswap32(x)
#define _be64(x) bswap64(x)
#define _le16(x) x
#define _le32(x) x
#define _le64(x) x
#else
#define _be16(x) x
#define _be32(x) x
#define _be64(x) x
#define _le16(x) __builtin_bswap16(x)
#define _le32(x) __builtin_bswap32(x)
#define _le64(x) __builtin_bswap64(x)
#endif

/**
 * Clears the DAIF I-bit, unmasking IRQ exceptions at the current EL.
 */
void irq_enable();

/**
 * Sets the DAIF I-bit, masking IRQ exceptions at the current EL.
 */
void irq_disable();

/**
 * Halts the CPU in a low-power loop using wfi, waking only to service
 * interrupts before returning to sleep.
 */
void halt() __attribute__((noreturn));

/**
 * Spins forever with interrupts disabled. Used for unrecoverable errors
 * where the system must stop completely.
 */
void hang() __attribute__((noreturn));
