#include <dtb.h>
#include <debug.h>
#include <stdlib.h>
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

void uart_printf(const char *format, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, format);
    uart_vprintf(format, args);
    __builtin_va_end(args);
}

void uart_vprintf(const char *format, __builtin_va_list args)
{
    char buff[1024];
    int n = vsprintf(buff, format, args);

    for (int i = 0; i < n; i++)
        uart_putc(buff[i]);
}

void uart_puts(const char *s)
{
    while (*s)
    {
        uart_putc(*s++);
    }
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
        dprintk("[uart] Initializing IRQ: %i\r\n", uart_irq);
        irq_register_handler(uart_irq, &uart_irq_handler);
        gic_enable_irq(uart_irq);
    }
    else
    {
        dprintk("[uart] IRQ not found!!\r\n");
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