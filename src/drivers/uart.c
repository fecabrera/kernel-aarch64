#include <dtb.h>
#include <arch/irq.h>
#include "uart.h"
#include "gic.h"

uint32_t uart_irq;

void uart_putc(char c)
{
    while (UART_FR & UART_FR_TXFF)
        ; // Wait if TX FIFO full
    UART_DR = c;
}

void uart_puts(const char *s)
{
    while (*s)
    {
        uart_putc(*s++);
    }
}

void uart_put_uint_base(uint64_t n, int base)
{
    if (n == 0)
    {
        uart_putc('0');
        return;
    }

    static const char digits[] = "0123456789abcdef";
    char buf[64];
    int i = 0;
    while (n > 0)
    {
        buf[i++] = digits[n % base];
        n /= base;
    }

    while (i--)
        uart_putc(buf[i]);
}

void uart_put_uint(uint64_t n)
{
    uart_put_uint_base(n, 10);
}

void uart_put_uint_hex(uint64_t n)
{
    uart_put_uint_base(n, 16);
}

void uart_init()
{
    // Disable UART before configuring
    UART_CR = UART_CR_DISABLED;

    // Set baud rate to 115200
    UART_IBRD = UART_BAUD_115200_IBRD;
    UART_FBRD = UART_BAUD_115200_FBRD;

    // 8-bit, no parity, 1 stop bit, enable FIFOs
    UART_LCR_H = UART_LCR_WLEN_8 | UART_LCR_FEN;

    // Set FIFO interrupt trigger level
    // IFLS = 0 means RX fires at 1/8 full (4 bytes) — low latency
    UART_IFLS = UART_IFLS_TX1_8;

    // Unmask RX and receive-timeout interrupts
    // RT fires if bytes sit in FIFO without filling the threshold
    UART_IMSC = UART_INT_RX | UART_INT_RT;

    // Enable UART, TX, RX
    UART_CR = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;

    if (dtb_get_uart_irq_number(&uart_irq) == 0)
    {
        uart_puts("[uart] Initializing IRQ: ");
        uart_put_uint(uart_irq);
        uart_puts("\r\n");

        irq_register_handler(uart_irq, &uart_irq_handler);
        gic_enable_irq(uart_irq);
    }
    else
    {
        uart_puts("[uart] IRQ not found!!\r\n");
    }
}

struct cpu_context *uart_irq_handler(struct cpu_context *ctx)
{
    // Read all available bytes (RX or timeout interrupt)
    while (!(UART_FR & UART_FR_RXFE))
    {
        uint32_t uat_dr = UART_DR;
        uint8_t c = (uint8_t)(uat_dr & UART_DR_DATA_MASK);

        switch (c)
        {
        case ASCII_ENTER:
            uart_puts("\r\n");
            break;
        case ASCII_BACKSPACE:
            uart_puts("\b \b");
            break;
        case ASCII_TAB:
            uart_puts("\t");
            break;
        default:
            uart_putc(c);
        }
    }

    // Clear the interrupt flags we handled
    UART_ICR = UART_INT_RX | UART_INT_RT;

    return ctx;
}