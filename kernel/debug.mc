import "interrupts/drivers/pl011";

/**
 * Formats a string and writes it to the UART. Always active.
 * Thin wrapper around pl011_vprintf; use for normal kernel log output.
 *
 * @param format: printf-style format string
 * @param ...:    variadic arguments matching the format specifiers
 */
fn printk(format: char*, ...) {
    let args: va_list;
    va_start(args, format);
    pl011_vprintf(format, args);
    va_end(args);
}

/**
 * Formats a string and writes it to the UART only when DEBUG is defined.
 * Compiles to a no-op in release builds.
 *
 * @param format: printf-style format string
 * @param ...:    variadic arguments matching the format specifiers
 */
fn dprintk(format: char*, ...) {
    @if (DEBUG) {
        let args: va_list;
        va_start(args, format);
        pl011_vprintf(format, args);
        va_end(args);
    }
}