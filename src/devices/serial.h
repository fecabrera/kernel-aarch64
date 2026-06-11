#pragma once

#include <stddef.h>
#include <stdint.h>

/**
 * Registers the serial device as an I/O module named "serial", creating a /dev/serial VFS node
 * backed by serial_read/serial_write. Must be called after io_init().
 */
void serial_init();

/**
 * io_handler_t read handler for /dev/serial. Reads count bytes from the PL011 UART by calling
 * pl011_getc in a blocking loop, spinning on wfi() until each byte is available.
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
 * io_handler_t write handler for /dev/serial. Writes count bytes from buffer to the PL011 UART one
 * byte at a time via pl011_putc.
 *
 * @param buffer:   input buffer
 * @param count:    number of bytes to write
 * @param offset:   unused
 * @param drv_info: unused
 *
 * @return count
 */
int serial_write(uint8_t *buffer, size_t count, size_t offset, uint64_t drv_info);
