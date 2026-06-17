#include "pl011.h"
#include "gic.h"
#include <arch/cpu.h>
#include <arch/irq.h>
#include <ctype.h>
#include <debug.h>
#include <dtb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uint32_t pl011_irq;

void pl011_putc(char c) {
    _wfi_while(PL011_FR & PL011_FR_TXFF); // Wait if TX FIFO full
    PL011_DR = c;
}

void pl011_printf(const char *format, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, format);
    pl011_vprintf(format, args);
    __builtin_va_end(args);
}

void pl011_vprintf(const char *format, __builtin_va_list args) {
    char tmp[32];

    while (*format) {
        if (*format != '%') {
            pl011_putc(*format++);
            continue;
        }

        format++;

        bool left_align = false;
        char pad_char = ' ';
        int pad_width = 0;
        if (*format == '-') {
            left_align = true;
            format++;
        }
        if (*format == '0') {
            pad_char = '0';
            format++;
        }
        while (*format >= '0' && *format <= '9')
            pad_width = pad_width * 10 + (*format++ - '0');

        switch (*format++) {
        case 'd':
        case 'i': {
            int val = __builtin_va_arg(args, int);
            itoa((int64_t)val, tmp, 10);
            int len = strlen(tmp);
            for (int i = len; i < pad_width; i++)
                pl011_putc(pad_char);
            for (char *p = tmp; *p; p++)
                pl011_putc(*p);
            break;
        }
        case 'u': {
            unsigned int val = __builtin_va_arg(args, unsigned int);
            itoa((int64_t)val, tmp, 10);
            int len = strlen(tmp);
            for (int i = len; i < pad_width; i++)
                pl011_putc(pad_char);
            for (char *p = tmp; *p; p++)
                pl011_putc(*p);
            break;
        }
        case 'X':
        case 'x': {
            unsigned int val = __builtin_va_arg(args, unsigned int);
            itoa((int64_t)val, tmp, 16);
            int len = strlen(tmp);
            for (int i = len; i < pad_width; i++)
                pl011_putc(pad_char);
            for (char *p = tmp; *p; p++)
                pl011_putc((*(format - 1) == 'X') ? toupper(*p) : *p);
            break;
        }
        case 's': {
            char *s = __builtin_va_arg(args, char *);
            int len = strlen(s);
            if (!left_align)
                for (int i = len; i < pad_width; i++)
                    pl011_putc(' ');
            while (*s)
                pl011_putc(*s++);
            if (left_align)
                for (int i = len; i < pad_width; i++)
                    pl011_putc(' ');
            break;
        }
        case 'c':
            pl011_putc((char)__builtin_va_arg(args, int));
            break;
        case '%':
            pl011_putc('%');
            break;
        default:
            pl011_putc('%');
            pl011_putc(*(format - 1));
            break;
        }
    }
}

void pl011_init() {
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

    if (dtb_get_pl011_irq_number(&pl011_irq) == 0) {
        dprintk("[pl011] Initializing IRQ: %i\n", pl011_irq);
        irq_register_handler(pl011_irq, &pl011_irq_handler);
        gic_enable_irq(pl011_irq);
    } else {
        dprintk("[pl011] IRQ not found!!\n");
    }
}

uint32_t pl011_getc(char *c) {
    uint32_t value = PL011_FR & PL011_FR_RXFE;
    if (!value)
        *c = (uint8_t)(PL011_DR & PL011_DR_DATA_MASK); // mask value
    return value;
}

struct cpu_context *pl011_irq_handler(__attribute__((unused)) int irq, struct cpu_context *ctx) {
    // pl011_read_input();

    // Clear the interrupt flags we handled
    PL011_ICR = PL011_INT_RX | PL011_INT_RT;

    return ctx;
}