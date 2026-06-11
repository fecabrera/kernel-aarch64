#pragma once

#include <stdint.h>

#define UART0_BASE 0x09000000UL

#define PL011_DR (*(volatile uint32_t *)(UART0_BASE + 0x000))    // Data
#define PL011_FR (*(volatile uint32_t *)(UART0_BASE + 0x018))    // Flags
#define PL011_IBRD (*(volatile uint32_t *)(UART0_BASE + 0x024))  // Baud integer
#define PL011_FBRD (*(volatile uint32_t *)(UART0_BASE + 0x028))  // Baud fraction
#define PL011_LCR_H (*(volatile uint32_t *)(UART0_BASE + 0x02C)) // Line control
#define PL011_CR (*(volatile uint32_t *)(UART0_BASE + 0x030))    // Control
#define PL011_IFLS (*(volatile uint32_t *)(UART0_BASE + 0x034))  // FIFO level select
#define PL011_IMSC (*(volatile uint32_t *)(UART0_BASE + 0x038))  // Interrupt mask
#define PL011_RIS (*(volatile uint32_t *)(UART0_BASE + 0x03C))   // Raw interrupt status
#define PL011_MIS (*(volatile uint32_t *)(UART0_BASE + 0x040))   // Masked interrupt status
#define PL011_ICR (*(volatile uint32_t *)(UART0_BASE + 0x044))   // Interrupt clear

// FR flags
#define PL011_FR_CTS (1 << 0)  // Clear to send
#define PL011_FR_DSR (1 << 1)  // Data set ready
#define PL011_FR_DCD (1 << 2)  // Data carrier detect
#define PL011_FR_BUSY (1 << 3) // UART transmitting
#define PL011_FR_RXFE (1 << 4) // RX FIFO empty
#define PL011_FR_TXFF (1 << 5) // TX FIFO full
#define PL011_FR_RXFF (1 << 6) // RX FIFO full
#define PL011_FR_TXFE (1 << 7) // TX FIFO empty

// LCR_H flags
#define PL011_LCR_BRK (1 << 0)  // Send break
#define PL011_LCR_PEN (1 << 1)  // Parity enable
#define PL011_LCR_EPS (1 << 2)  // Even parity (0 = odd, 1 = even)
#define PL011_LCR_STP2 (1 << 3) // Two stop bits
#define PL011_LCR_FEN (1 << 4)  // FIFO enable
#define PL011_LCR_SPS (1 << 7)  // Stick parity

// WLEN — word length (bits 6:5)
#define PL011_LCR_WLEN_5 (0 << 5) // 5-bit words
#define PL011_LCR_WLEN_6 (1 << 5) // 6-bit words
#define PL011_LCR_WLEN_7 (2 << 5) // 7-bit words
#define PL011_LCR_WLEN_8 (3 << 5) // 8-bit words (standard)

// IBRD / FBRD — baud rate divisor (assumes 24MHz UARTCLK, as on QEMU virt)
// Formula: divisor = UARTCLK / (16 * baud), IBRD = integer part, FBRD =
// round(fraction * 64)
#define PL011_BAUD_9600_IBRD 156
#define PL011_BAUD_9600_FBRD 16
#define PL011_BAUD_38400_IBRD 39
#define PL011_BAUD_38400_FBRD 4
#define PL011_BAUD_57600_IBRD 26
#define PL011_BAUD_57600_FBRD 3
#define PL011_BAUD_115200_IBRD 13
#define PL011_BAUD_115200_FBRD 1

// CR flags
#define PL011_CR_DISABLED (0 << 0) // UART disabled
#define PL011_CR_UARTEN (1 << 0)   // UART enable
#define PL011_CR_SIREN (1 << 1)    // SIR enable (IrDA)
#define PL011_CR_SIRLP (1 << 2)    // IrDA SIR low-power mode
#define PL011_CR_LBE (1 << 7)      // Loopback enable
#define PL011_CR_TXE (1 << 8)      // Transmit enable
#define PL011_CR_RXE (1 << 9)      // Receive enable
#define PL011_CR_DTR (1 << 10)     // Data transmit ready
#define PL011_CR_RTS (1 << 11)     // Request to send
#define PL011_CR_RTSEN (1 << 14)   // RTS hardware flow control enable
#define PL011_CR_CTSEN (1 << 15)   // CTS hardware flow control enable

// IFLS flags — TX level (bits 2:0), RX level (bits 5:3)
#define PL011_IFLS_TX1_8 (0 << 0) // TX interrupt at 1/8 full
#define PL011_IFLS_TX1_4 (1 << 0) // TX interrupt at 1/4 full
#define PL011_IFLS_TX1_2 (2 << 0) // TX interrupt at 1/2 full (default)
#define PL011_IFLS_TX3_4 (3 << 0) // TX interrupt at 3/4 full
#define PL011_IFLS_TX7_8 (4 << 0) // TX interrupt at 7/8 full
#define PL011_IFLS_RX1_8 (0 << 3) // RX interrupt at 1/8 full
#define PL011_IFLS_RX1_4 (1 << 3) // RX interrupt at 1/4 full
#define PL011_IFLS_RX1_2 (2 << 3) // RX interrupt at 1/2 full (default)
#define PL011_IFLS_RX3_4 (3 << 3) // RX interrupt at 3/4 full
#define PL011_IFLS_RX7_8 (4 << 3) // RX interrupt at 7/8 full

// Interrupt bits (same bit position in IMSC, RIS, MIS, ICR)
#define PL011_INT_RX (1 << 4) // Receive interrupt
#define PL011_INT_TX (1 << 5) // Transmit interrupt
#define PL011_INT_RT (1 << 6) // Receive timeout

// DR masks
#define PL011_DR_DATA_MASK 0xFF

#define RX_BUF_SIZE 1024

/**
 * Writes a single character to the UART, blocking until the TX FIFO has space.
 *
 * @param c: character to transmit
 */
void pl011_putc(char c);

/**
 * Formats a string and writes it to the UART.
 * Supports: %d/%i (signed int), %u (unsigned int), %x (hex), %s (string), %c (char), %%.
 *
 * @param format: printf-style format string
 * @param ...: variadic arguments matching the format specifiers
 */
void pl011_printf(const char *format, ...);

/**
 * Formats a string and writes it to the UART using a pre-initialized va_list.
 * Supports: %d/%i (signed int), %u (unsigned int), %x (hex), %s (string), %c (char), %%.
 *
 * @param format: printf-style format string
 * @param args:   variadic argument list (must be initialized by the caller)
 */
void pl011_vprintf(const char *format, __builtin_va_list args);

/**
 * Initializes the PL011 UART: sets baud rate to 115200 8N1, enables FIFOs, unmasks RX interrupts,
 * and registers the IRQ with the GIC.
 */
void pl011_init();

/**
 * Reads the next byte from the RX FIFO without blocking.
 *
 * @param c: output byte; written only if the RX FIFO is non-empty
 *
 * @return 0 if a byte was read, non-zero (PL011_FR_RXFE set) if the RX FIFO is empty
 */
uint32_t pl011_getc(char *c);

/**
 * IRQ handler for UART RX and receive-timeout interrupts.
 * Drains the RX FIFO into the ring buffer and clears the interrupt flags. Registered with
 * irq_register_handler at pl011_init time.
 *
 * @param irq: IRQ ID passed by the dispatcher (unused)
 * @param ctx: saved register frame from the interrupted context
 *
 * @return ctx unchanged (no context switch).
 */
struct cpu_context *pl011_irq_handler(int irq, struct cpu_context *ctx);
