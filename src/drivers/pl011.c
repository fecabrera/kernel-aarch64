#include <dtb.h>
#include <debug.h>
#include <stdlib.h>
#include <arch/irq.h>
#include "pl011.h"
#include "gic.h"

static uint32_t pl011_irq;

void pl011_putc(char c)
{
    while (PL011_FR & PL011_FR_TXFF)
        ; // Wait if TX FIFO full
    PL011_DR = c;
}

void pl011_printf(const char *format, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, format);
    pl011_vprintf(format, args);
    __builtin_va_end(args);
}

void pl011_vprintf(const char *format, __builtin_va_list args)
{
    char buff[1024];
    int n = vsprintf(buff, format, args);

    for (int i = 0; i < n; i++)
        pl011_putc(buff[i]);
}

void pl011_puts(const char *s)
{
    while (*s)
    {
        pl011_putc(*s++);
    }
}

void pl011_init()
{
    // Disable UART before configuring
    PL011_CR = PL011_CR_DISABLED;

    // Set baud rate to 115200
    PL011_IBRD = PL011_BAUD_115200_IBRD;
    PL011_FBRD = PL011_BAUD_115200_FBRD;

    // 8-bit, no parity, 1 stop bit, enable FIFOs
    PL011_LCR_H = PL011_LCR_WLEN_8 | PL011_LCR_FEN;

    // Set FIFO interrupt trigger level
    // IFLS = 0 means RX fires at 1/8 full (4 bytes) — low latency
    PL011_IFLS = PL011_IFLS_TX1_8;

    // Unmask RX and receive-timeout interrupts
    // RT fires if bytes sit in FIFO without filling the threshold
    PL011_IMSC = PL011_INT_RX | PL011_INT_RT;

    // Enable UART, TX, RX
    PL011_CR = PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE;

    if (dtb_get_pl011_irq_number(&pl011_irq) == 0)
    {
        dprintk("[pl011] Initializing IRQ: %i\r\n", pl011_irq);
        irq_register_handler(pl011_irq, &pl011_irq_handler);
        gic_enable_irq(pl011_irq);
    }
    else
    {
        dprintk("[pl011] IRQ not found!!\r\n");
    }
}

struct cpu_context *pl011_irq_handler(__attribute__((unused)) int irq, struct cpu_context *ctx)
{
    // Read all available bytes (RX or timeout interrupt)
    while (!(PL011_FR & PL011_FR_RXFE))
    {
        uint32_t uat_dr = PL011_DR;
        uint8_t c = (uint8_t)(uat_dr & PL011_DR_DATA_MASK);

        switch (c)
        {
        case ASCII_ENTER:
            pl011_puts("\r\n");
            break;
        case ASCII_BACKSPACE:
            pl011_puts("\b \b");
            break;
        case ASCII_TAB:
            pl011_puts("\t");
            break;
        default:
            pl011_putc(c);
        }
    }

    // Clear the interrupt flags we handled
    PL011_ICR = PL011_INT_RX | PL011_INT_RT;

    return ctx;
}