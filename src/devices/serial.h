#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Registers a "/dev/serial" I/O module backed by the PL011 UART.
 * Must be called after io_init().
 */
void serial_init();

/**
 * Reads count bytes from the PL011 RX FIFO into buffer, blocking on wfi() until each byte arrives.
 * offset and drv_info are unused.
 *
 * @param buffer:   output buffer (must hold at least count bytes)
 * @param count:    number of bytes to read
 * @param offset:   unused
 * @param drv_info: unused
 *
 * @return count
 */
int serial_read(uint8_t *buffer, size_t count, size_t offset, uint64_t drv_info);

/**
 * Writes count bytes from buffer to the PL011 TX FIFO via pl011_putc.
 * offset and drv_info are unused.
 *
 * @param buffer:   input buffer
 * @param count:    number of bytes to write
 * @param offset:   unused
 * @param drv_info: unused
 *
 * @return count
 */
int serial_write(uint8_t *buffer, size_t count, size_t offset, uint64_t drv_info);
