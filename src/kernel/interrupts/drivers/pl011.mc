import "debug";
import "cpu";
import "dtb";
import "libc/stdio";
import "interrupts/irq";
import "interrupts/gic";

/**
 * PL011 UART register block, memory-mapped at UART0_BASE. All registers are
 * 32-bit; reserved gaps (RSR/ECR, ILPR, etc.) are skipped to keep the named
 * registers at their architectural offsets. Access them through the PL011
 * pointer (e.g. PL011->dr).
 */
@volatile
struct pl011_regs {
    dr: uint32;             // 0x000 Data
    _reserved0: uint32[5];  // 0x004–0x014
    fr: uint32;             // 0x018 Flags
    _reserved1: uint32[2];  // 0x01C–0x020
    ibrd: uint32;           // 0x024 Baud integer
    fbrd: uint32;           // 0x028 Baud fraction
    lcr_h: uint32;          // 0x02C Line control
    cr: uint32;             // 0x030 Control
    ifls: uint32;           // 0x034 FIFO level select
    imsc: uint32;           // 0x038 Interrupt mask
    ris: uint32;            // 0x03C Raw interrupt status
    mis: uint32;            // 0x040 Masked interrupt status
    icr: uint32;            // 0x044 Interrupt clear
}

const UART0_BASE = 0x09000000;

// FR flags
const PL011_FR_CTS = (1 << 0);  // Clear to send
const PL011_FR_DSR = (1 << 1);  // Data set ready
const PL011_FR_DCD = (1 << 2);  // Data carrier detect
const PL011_FR_BUSY = (1 << 3); // UART transmitting
const PL011_FR_RXFE = (1 << 4); // RX FIFO empty
const PL011_FR_TXFF = (1 << 5); // TX FIFO full
const PL011_FR_RXFF = (1 << 6); // RX FIFO full
const PL011_FR_TXFE = (1 << 7); // TX FIFO empty

// LCR_H flags
const PL011_LCR_BRK = (1 << 0);  // Send break
const PL011_LCR_PEN = (1 << 1);  // Parity enable
const PL011_LCR_EPS = (1 << 2);  // Even parity (0 = odd, 1 = even)
const PL011_LCR_STP2 = (1 << 3); // Two stop bits
const PL011_LCR_FEN = (1 << 4);  // FIFO enable
const PL011_LCR_SPS = (1 << 7);  // Stick parity

// WLEN — word length (bits 6:5)
const PL011_LCR_WLEN_5 = (0 << 5); // 5-bit words
const PL011_LCR_WLEN_6 = (1 << 5); // 6-bit words
const PL011_LCR_WLEN_7 = (2 << 5); // 7-bit words
const PL011_LCR_WLEN_8 = (3 << 5); // 8-bit words (standard)

// IBRD / FBRD — baud rate divisor (assumes 24MHz UARTCLK, as on QEMU virt)
// Formula: divisor = UARTCLK / (16 * baud), IBRD = integer part, FBRD =
// round(fraction * 64)
const PL011_BAUD_9600_IBRD = 156;
const PL011_BAUD_9600_FBRD = 16;
const PL011_BAUD_38400_IBRD = 39;
const PL011_BAUD_38400_FBRD = 4;
const PL011_BAUD_57600_IBRD = 26;
const PL011_BAUD_57600_FBRD = 3;
const PL011_BAUD_115200_IBRD = 13;
const PL011_BAUD_115200_FBRD = 1;

// CR flags
const PL011_CR_DISABLED = (0 << 0); // UART disabled
const PL011_CR_UARTEN = (1 << 0);   // UART enable
const PL011_CR_SIREN = (1 << 1);    // SIR enable (IrDA)
const PL011_CR_SIRLP = (1 << 2);    // IrDA SIR low-power mode
const PL011_CR_LBE = (1 << 7);      // Loopback enable
const PL011_CR_TXE = (1 << 8);      // Transmit enable
const PL011_CR_RXE = (1 << 9);      // Receive enable
const PL011_CR_DTR = (1 << 10);     // Data transmit ready
const PL011_CR_RTS = (1 << 11);     // Request to send
const PL011_CR_RTSEN = (1 << 14);   // RTS hardware flow control enable
const PL011_CR_CTSEN = (1 << 15);   // CTS hardware flow control enable

// IFLS flags — TX level (bits 2:0), RX level (bits 5:3)
const PL011_IFLS_TX1_8 = (0 << 0); // TX interrupt at 1/8 full
const PL011_IFLS_TX1_4 = (1 << 0); // TX interrupt at 1/4 full
const PL011_IFLS_TX1_2 = (2 << 0); // TX interrupt at 1/2 full (default)
const PL011_IFLS_TX3_4 = (3 << 0); // TX interrupt at 3/4 full
const PL011_IFLS_TX7_8 = (4 << 0); // TX interrupt at 7/8 full
const PL011_IFLS_RX1_8 = (0 << 3); // RX interrupt at 1/8 full
const PL011_IFLS_RX1_4 = (1 << 3); // RX interrupt at 1/4 full
const PL011_IFLS_RX1_2 = (2 << 3); // RX interrupt at 1/2 full (default)
const PL011_IFLS_RX3_4 = (3 << 3); // RX interrupt at 3/4 full
const PL011_IFLS_RX7_8 = (4 << 3); // RX interrupt at 7/8 full

// Interrupt bits (same bit position in IMSC, RIS, MIS, ICR)
const PL011_INT_RX = (1 << 4); // Receive interrupt
const PL011_INT_TX = (1 << 5); // Transmit interrupt
const PL011_INT_RT = (1 << 6); // Receive timeout

// DR masks
const PL011_DR_DATA_MASK = 0xFF;

// const RX_BUF_SIZE = 1024;

@static let PL011 = UART0_BASE as struct pl011_regs*;

@static let pl011_irq: uint32;

/**
 * Initializes the PL011 UART: sets baud rate to 115200 8N1, enables FIFOs, unmasks RX interrupts,
 * and registers the IRQ with the GIC.
 */
fn pl011_init() {
    // PL011 = UART0_BASE as struct pl011_regs*;

    // Disable UART before configuring
    PL011->cr = PL011_CR_DISABLED;

    // Set baud rate to 115200
    PL011->ibrd = PL011_BAUD_115200_IBRD;
    PL011->fbrd = PL011_BAUD_115200_FBRD;

    // 8-bit, no parity, 1 stop bit, enable FIFOs
    PL011->lcr_h = PL011_LCR_WLEN_8 | PL011_LCR_FEN;

    // Set FIFO interrupt trigger level
    // IFLS = 0 means RX fires at 1/8 full (4 bytes) — low latency
    PL011->ifls = PL011_IFLS_TX1_8;

    // Unmask RX and receive-timeout interrupts
    // RT fires if bytes sit in FIFO without filling the threshold
    PL011->imsc = PL011_INT_RX | PL011_INT_RT;

    // Enable UART, TX, RX
    PL011->cr = PL011_CR_UARTEN | PL011_CR_TXE | PL011_CR_RXE;

    if (dtb_get_pl011_irq_number(&pl011_irq) == 0) {
        dprintk("[pl011] Initializing IRQ: %i\n", pl011_irq);
        irq_register_handler(pl011_irq, pl011_irq_handler);
        gic_enable_irq(pl011_irq);
    } else {
        dprintk("[pl011] IRQ not found!!\n");
    }
}

/**
 * Reads the next byte from the RX FIFO without blocking.
 *
 * @param c: output byte; written only if the RX FIFO is non-empty
 *
 * @return 0 if a byte was read, non-zero (PL011_FR_RXFE set) if the RX FIFO is empty
 */
fn pl011_getc(c: uint8*) -> uint32 {
    let value = PL011->fr & PL011_FR_RXFE;
    if (value == 0)
        *c = (PL011->dr & PL011_DR_DATA_MASK) as uint8; // mask value
    return value;
}

/**
 * Writes a single character to the UART, spinning via wfi() until the TX FIFO has space.
 *
 * @param c: character to transmit
 */
fn pl011_putc(c: uint8) {
    while(PL011->fr & PL011_FR_TXFF)
        wfi(); // Wait if TX FIFO full
    
    PL011->dr = c as uint32;
}

/**
 * Formats a string and writes it to the UART. Thin variadic wrapper around
 * pl011_vprintf; see vsprintfcb/stb_sprintf for the supported format grammar
 * (full printf flags/width/precision/length and d/i/u/o/x/X/c/s/p/b; floats
 * disabled).
 *
 * @param format: printf-style format string
 * @param ...: variadic arguments matching the format specifiers
 */
fn pl011_printf(format: uint8*, ...) {
    let args: va_list;
    va_start(args, format);
    pl011_vprintf(format, args);
    va_end(args);
}

/**
 * Formats a string and writes it to the UART using a pre-initialized va_list.
 * Drives stb_sprintf's vsprintfcb with pl011_cb, so output is streamed to the
 * UART in STB_SPRINTF_MIN chunks without a full output buffer. Supports the full
 * stb_sprintf grammar (flags, width, precision, length modifiers, and the
 * d/i/u/o/x/X/c/s/p/b conversions); %f/%e/%g/%a are disabled in this build.
 *
 * @param format: printf-style format string
 * @param args:   variadic argument list (must be initialized by the caller)
 */
fn pl011_vprintf(format: uint8*, args: va_list) {
    let c: struct stbsp__context;
    c.length = 0;
    // stb writes into c.tmp (the STB_SPRINTF_MIN scratch buffer) and hands &c
    // back to pl011_cb as `user`; pl011_cb flushes c.tmp to the UART.
    vsprintfcb(pl011_cb, &c, c.tmp, format, args);
}

/**
 * vsprintfcb callback for pl011_vprintf. Writes the count formatted bytes in buf
 * to the UART via pl011_putc, accumulates the total in c->length, and returns
 * c->tmp so stb keeps formatting into the same scratch buffer.
 *
 * @param buf:   chunk of formatted output to flush
 * @param c:     context whose tmp buffer is reused for the next chunk
 * @param count: number of valid bytes in buf
 *
 * @return c->tmp, the scratch buffer for stb's next chunk
 */
@private
fn pl011_cb(buf: uint8*, c: struct stbsp__context*, count: int32) -> uint8* {
    let i: int32 = 0;
    while (i < count) {
        pl011_putc(buf[i]);
        i = i + 1;
    }
    c->length = c->length + count;
    return c->tmp;
}

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
fn pl011_irq_handler(irq: uint32, ctx: struct cpu_context*) -> struct cpu_context* {
    // Clear the interrupt flags we handled
    PL011->icr = PL011_INT_RX | PL011_INT_RT;

    return ctx;
}
