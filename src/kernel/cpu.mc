// SPSR_EL1 — exception level and stack pointer selection (bits 3:0)
const SPSR_EL0t = 0x00; // EL0, SP_EL0
const SPSR_EL1t = 0x04; // EL1, SP_EL0
const SPSR_EL1h = 0x05; // EL1, SP_EL1 (standard kernel mode)
const SPSR_EL2t = 0x08; // EL2, SP_EL0
const SPSR_EL2h = 0x09; // EL2, SP_EL2

// SPSR_EL1 — interrupt mask bits (DAIF, bits 9:6)
const SPSR_D = (1 << 9); // Debug exceptions masked
const SPSR_A = (1 << 8); // SError masked
const SPSR_I = (1 << 7); // IRQ masked
const SPSR_F = (1 << 6); // FIQ masked
const SPSR_MASKS_ALL = (SPSR_D | SPSR_A | SPSR_I | SPSR_F);

// cntp_ctl_el0 bits
const CNTP_CTL_ENABLE = (1 << 0);  // Timer enabled
const CNTP_CTL_IMASK = (1 << 1);   // Interrupt masked (suppress IRQ)
const CNTP_CTL_ISTATUS = (1 << 2); // Interrupt status (read-only, 1 = condition met)

struct cpu_context {
    x: uint64[30];
    lr: uint64;
    elr: uint64;
    spsr: uint64;
}

/**
 * Reverses the byte order of a 16-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its two bytes reversed
 */
@extern fn bswap16(x: uint16) -> uint16;

/**
 * Reverses the byte order of a 32-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its four bytes reversed
 */
@extern fn bswap32(x: uint32) -> uint32;

/**
 * Reverses the byte order of a 64-bit value, converting between big-endian
 * and little-endian representations.
 *
 * @param x: value to byte-swap
 *
 * @return x with its eight bytes reversed
 */
@extern fn bswap64(x: uint64) -> uint64;

/**
 * Converts a 16-bit value between host and big-endian byte order. On the
 * little-endian AArch64 host this swaps the two bytes; the conversion is its
 * own inverse, so the same call reads or writes big-endian data.
 *
 * @param x: value to convert
 *
 * @return x with host and big-endian byte order exchanged
 */
@inline fn be16(x: uint16) -> uint16 { return bswap16(x); }

/**
 * Converts a 32-bit value between host and big-endian byte order. On the
 * little-endian AArch64 host this reverses all four bytes; the conversion is
 * its own inverse, so the same call reads or writes big-endian data.
 *
 * @param x: value to convert
 *
 * @return x with host and big-endian byte order exchanged
 */
@inline fn be32(x: uint32) -> uint32 { return bswap32(x); }

/**
 * Converts a 64-bit value between host and big-endian byte order. On the
 * little-endian AArch64 host this reverses all eight bytes; the conversion is
 * its own inverse, so the same call reads or writes big-endian data.
 *
 * @param x: value to convert
 *
 * @return x with host and big-endian byte order exchanged
 */
@inline fn be64(x: uint64) -> uint64 { return bswap64(x); }

/**
 * Converts a 16-bit value between host and little-endian byte order. The host
 * is already little-endian, so this is a no-op provided for symmetry with be16.
 *
 * @param x: value to convert
 *
 * @return x unchanged
 */
@inline fn le16(x: uint16) -> uint16 { return x; }

/**
 * Converts a 32-bit value between host and little-endian byte order. The host
 * is already little-endian, so this is a no-op provided for symmetry with be32.
 *
 * @param x: value to convert
 *
 * @return x unchanged
 */
@inline fn le32(x: uint32) -> uint32 { return x; }

/**
 * Converts a 64-bit value between host and little-endian byte order. The host
 * is already little-endian, so this is a no-op provided for symmetry with be64.
 *
 * @param x: value to convert
 *
 * @return x unchanged
 */
@inline fn le64(x: uint64) -> uint64 { return x; }

/**
 * Executes the WFE (Wait For Event) instruction, suspending the CPU until an
 * event or interrupt is signalled. Less aggressive than WFI — wakes on SEV
 * (send event) from another core as well as interrupts.
 */
@extern fn wfe();

/**
 * Executes the WFI (Wait For Interrupt) instruction, suspending the CPU until
 * an interrupt is pending. Used in spin loops to avoid busy-waiting.
 */
@extern fn wfi();

/**
 * Clears the DAIF I-bit, unmasking IRQ exceptions at the current EL.
 */
@extern fn irq_enable();

/**
 * Sets the DAIF I-bit, masking IRQ exceptions at the current EL.
 */
@extern fn irq_disable();

/**
 * Halts the CPU in a low-power loop using wfi, waking only to service
 * interrupts before returning to sleep.
 */
@extern fn halt();

/**
 * Spins forever with interrupts disabled. Used for unrecoverable errors
 * where the system must stop completely.
 */
@extern fn hang();

/**
 * Reads the physical count register (cntpct_el0).
 * Increments at the frequency reported by cntfrq_el0 (typically 24 MHz on
 * QEMU virt). Divide by get_cntfrq_el0() to convert ticks to seconds.
 *
 * @return Current 64-bit physical counter value.
 */
@extern fn get_cntpct_el0() -> uint64;

/**
 * Reads the timer frequency register (cntfrq_el0).
 *
 * @return Timer frequency in Hz as set by firmware.
 */
@extern fn get_cntfrq_el0() -> uint64;

/**
 * Writes the EL1 physical timer value register (cntp_tval_el0).
 * Starts a countdown from value; the timer fires IRQ when it reaches zero.
 * Must be rewritten each tick to reload the countdown.
 *
 * @param value: number of timer ticks until the next interrupt
 */
@extern fn set_cntp_tval_el0(value: uint64);

/**
 * Reads the EL1 physical timer control register (cntp_ctl_el0).
 * Check CNTP_CTL_ISTATUS to determine if the timer condition has been met.
 *
 * @return Current value of cntp_ctl_el0.
 */
@extern fn get_cntp_ctl_el0() -> uint64;

/**
 * Writes the EL1 physical timer control register (cntp_ctl_el0).
 * Use CNTP_CTL_ENABLE, CNTP_CTL_IMASK, and CNTP_CTL_ISTATUS flags.
 *
 * @param value: value to write (combination of CNTP_CTL_* flags)
 */
@extern fn set_cntp_ctl_el0(value: uint64);

/**
 * Sets the EL1 exception vector base register (vbar_el1) to the given table
 * address, followed by an instruction sync barrier so the change takes effect.
 *
 * @param table: address of the exception vector table to install
 */
@extern fn set_vbar_el1(table: uint64);