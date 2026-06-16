import "debug";
import "cpu";
import "io";
import "drivers/pl011";

/**
 * Registers a "/dev/serial" I/O module backed by the PL011 UART.
 * Must be called after io_init().
 */
fn serial_init() {
    let mod_name: uint8* = "serial";
    dprintk("[serial] adding \"/dev/%s\"\r\n", mod_name);
    io_register_module(mod_name, 0, serial_read, serial_write);
    dprintk("[serial] \"/dev/%s\" added successfully!\n", mod_name);
}

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
fn serial_read(buffer: uint8*, count: uint64, offset: uint64, drv_info: uint64) -> int32 {
    let c: uint8;
    let i: uint64 = 0;
    while (i < count) {
        defer i = i + 1;
        // _wfi_while(pl011_getc(&c));
        while (pl011_getc(&c))
            wfi();
        buffer[i] = c;
    }
    return count as int32;
}

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
fn serial_write(buffer: uint8*, count: uint64, offset: uint64, drv_info: uint64) -> int32 {
    let i: uint64 = 0;
    while (i < count) {
        defer i = i + 1;
        pl011_putc(buffer[i]);
    }
    return count as int32;
}