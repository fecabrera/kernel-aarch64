#pragma once

/**
 * Registers a "/dev/serial" I/O module backed by the PL011 UART.
 * Must be called after io_init().
 */
void serial_init();