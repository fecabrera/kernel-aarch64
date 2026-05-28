#ifndef UART_H
#define UART_H

#include <types.h>

#define UART0_BASE 0x09000000UL

#define UART_DR (*(volatile uint32_t *)(UART0_BASE + 0x000))    // Data
#define UART_FR (*(volatile uint32_t *)(UART0_BASE + 0x018))    // Flags
#define UART_IBRD (*(volatile uint32_t *)(UART0_BASE + 0x024))  // Baud integer
#define UART_FBRD (*(volatile uint32_t *)(UART0_BASE + 0x028))  // Baud fraction
#define UART_LCR_H (*(volatile uint32_t *)(UART0_BASE + 0x02C)) // Line control
#define UART_CR (*(volatile uint32_t *)(UART0_BASE + 0x030))    // Control
#define UART_IFLS (*(volatile uint32_t *)(UART0_BASE + 0x034))  // FIFO level select
#define UART_IMSC (*(volatile uint32_t *)(UART0_BASE + 0x038))  // Interrupt mask
#define UART_RIS (*(volatile uint32_t *)(UART0_BASE + 0x03C))   // Raw interrupt status
#define UART_MIS (*(volatile uint32_t *)(UART0_BASE + 0x040))   // Masked interrupt status
#define UART_ICR (*(volatile uint32_t *)(UART0_BASE + 0x044))   // Interrupt clear

// FR flags
#define UART_FR_CTS (1 << 0)  // Clear to send
#define UART_FR_DSR (1 << 1)  // Data set ready
#define UART_FR_DCD (1 << 2)  // Data carrier detect
#define UART_FR_BUSY (1 << 3) // UART transmitting
#define UART_FR_RXFE (1 << 4) // RX FIFO empty
#define UART_FR_TXFF (1 << 5) // TX FIFO full
#define UART_FR_RXFF (1 << 6) // RX FIFO full
#define UART_FR_TXFE (1 << 7) // TX FIFO empty

// LCR_H flags
#define UART_LCR_BRK (1 << 0)  // Send break
#define UART_LCR_PEN (1 << 1)  // Parity enable
#define UART_LCR_EPS (1 << 2)  // Even parity (0 = odd, 1 = even)
#define UART_LCR_STP2 (1 << 3) // Two stop bits
#define UART_LCR_FEN (1 << 4)  // FIFO enable
#define UART_LCR_SPS (1 << 7)  // Stick parity

// WLEN — word length (bits 6:5)
#define UART_LCR_WLEN_5 (0 << 5) // 5-bit words
#define UART_LCR_WLEN_6 (1 << 5) // 6-bit words
#define UART_LCR_WLEN_7 (2 << 5) // 7-bit words
#define UART_LCR_WLEN_8 (3 << 5) // 8-bit words (standard)

// IBRD / FBRD — baud rate divisor (assumes 24MHz UARTCLK, as on QEMU virt)
// Formula: divisor = UARTCLK / (16 * baud), IBRD = integer part, FBRD = round(fraction * 64)
#define UART_BAUD_9600_IBRD 156
#define UART_BAUD_9600_FBRD 16
#define UART_BAUD_38400_IBRD 39
#define UART_BAUD_38400_FBRD 4
#define UART_BAUD_57600_IBRD 26
#define UART_BAUD_57600_FBRD 3
#define UART_BAUD_115200_IBRD 13
#define UART_BAUD_115200_FBRD 1

// CR flags
#define UART_CR_DISABLED (0 << 0) // UART disabled
#define UART_CR_UARTEN (1 << 0)   // UART enable
#define UART_CR_SIREN (1 << 1)    // SIR enable (IrDA)
#define UART_CR_SIRLP (1 << 2)    // IrDA SIR low-power mode
#define UART_CR_LBE (1 << 7)      // Loopback enable
#define UART_CR_TXE (1 << 8)      // Transmit enable
#define UART_CR_RXE (1 << 9)      // Receive enable
#define UART_CR_DTR (1 << 10)     // Data transmit ready
#define UART_CR_RTS (1 << 11)     // Request to send
#define UART_CR_RTSEN (1 << 14)   // RTS hardware flow control enable
#define UART_CR_CTSEN (1 << 15)   // CTS hardware flow control enable

// IFLS flags — TX level (bits 2:0), RX level (bits 5:3)
#define UART_IFLS_TX1_8 (0 << 0) // TX interrupt at 1/8 full
#define UART_IFLS_TX1_4 (1 << 0) // TX interrupt at 1/4 full
#define UART_IFLS_TX1_2 (2 << 0) // TX interrupt at 1/2 full (default)
#define UART_IFLS_TX3_4 (3 << 0) // TX interrupt at 3/4 full
#define UART_IFLS_TX7_8 (4 << 0) // TX interrupt at 7/8 full
#define UART_IFLS_RX1_8 (0 << 3) // RX interrupt at 1/8 full
#define UART_IFLS_RX1_4 (1 << 3) // RX interrupt at 1/4 full
#define UART_IFLS_RX1_2 (2 << 3) // RX interrupt at 1/2 full (default)
#define UART_IFLS_RX3_4 (3 << 3) // RX interrupt at 3/4 full
#define UART_IFLS_RX7_8 (4 << 3) // RX interrupt at 7/8 full

// Interrupt bits (same bit position in IMSC, RIS, MIS, ICR)
#define UART_INT_RX (1 << 4) // Receive interrupt
#define UART_INT_TX (1 << 5) // Transmit interrupt
#define UART_INT_RT (1 << 6) // Receive timeout

// DR masks
#define UART_DR_DATA_MASK 0xFF

// ASCII control characters
#define ASCII_TAB 0x09
#define ASCII_ENTER 0x0D
#define ASCII_BACKSPACE 0x7F

#define RX_BUF_SIZE 1024

/**
 * Writes a single character to the UART, blocking until the TX FIFO has space.
 *
 * @param c: character to transmit
 */
void uart_putc(char c);

/**
 * Formats a string and writes it to the UART.
 * Supports: %d/%i (signed int), %u (unsigned int), %x (hex), %s (string), %c (char), %%.
 *
 * @param format: printf-style format string
 * @param ...: variadic arguments matching the format specifiers
 */
void uart_printf(const char *format, ...);

/**
 * Writes a null-terminated string to the UART.
 *
 * @param s: pointer to the null-terminated string to transmit
 */
void uart_puts(const char *s);

/**
 * Writes an unsigned 64-bit integer to the UART in the given base.
 * Supports bases 2–16; digits above 9 are lowercase (a–f).
 *
 * @param n: value to transmit
 * @param base: numeric base (e.g. 2, 8, 10, 16)
 */
void uart_put_uint_base(uint64_t n, int base);

/**
 * Writes an unsigned 64-bit integer to the UART as a decimal string.
 *
 * @param n: value to transmit
 */
void uart_put_uint(uint64_t n);

/**
 * Writes an unsigned 64-bit integer to the UART as a lowercase hex string,
 * without a leading "0x" prefix.
 *
 * @param n: value to transmit
 */
void uart_put_uint_hex(uint64_t n);

/**
 * Initializes the PL011 UART: sets baud rate to 115200 8N1, enables FIFOs,
 * unmasks RX interrupts, and registers the IRQ with the GIC.
 */
void uart_init();

/**
 * IRQ handler for UART RX and receive-timeout interrupts.
 * Drains the RX FIFO into the ring buffer and clears the interrupt flags.
 * Registered with irq_register_handler at uart_init time.
 *
 * @param ctx: saved register frame from the interrupted context
 *
 * @return ctx unchanged (no context switch).
 */
struct cpu_context *uart_irq_handler(struct cpu_context *ctx);

#endif // UART_H
