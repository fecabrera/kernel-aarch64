#pragma once

/**
 * Formats a string and writes it to the UART. Always active.
 * Thin wrapper around pl011_vprintf; use for normal kernel log output.
 *
 * @param format: printf-style format string
 * @param ...:    variadic arguments matching the format specifiers
 */
void printk(const char *format, ...);

/**
 * Formats a string and writes it to the UART only when DEBUG is defined.
 * Compiles to a no-op in release builds.
 *
 * @param format: printf-style format string
 * @param ...:    variadic arguments matching the format specifiers
 */
void dprintk(const char *format, ...);
