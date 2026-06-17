/**
 * Writes a single character to the UART, spinning via wfi() until the TX FIFO has space.
 *
 * @param c: character to transmit
 */
@extern fn pl011_putc(c: uint8);

/**
 * Formats a string and writes it to the UART.
 * Supports: %d/%i (signed int), %u (unsigned int), %x/%X (hex lower/upper), %s (string),
 * %c (char), %%. Width modifier N supported for all specifiers (e.g. %8d, %08x). For strings,
 * %-Ns left-aligns within width N (padding on the right); %Ns right-aligns (padding on the left).
 *
 * @param format: printf-style format string
 * @param ...: variadic arguments matching the format specifiers
 */
@extern fn pl011_printf(format: uint8*, ...);

/**
 * Initializes the PL011 UART: sets baud rate to 115200 8N1, enables FIFOs, unmasks RX interrupts,
 * and registers the IRQ with the GIC.
 */
@extern fn pl011_init();

/**
 * Reads the next byte from the RX FIFO without blocking.
 *
 * @param c: output byte; written only if the RX FIFO is non-empty
 *
 * @return 0 if a byte was read, non-zero (PL011_FR_RXFE set) if the RX FIFO is empty
 */
@extern fn pl011_getc(c: uint8*) -> uint32;