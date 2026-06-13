import "arch/cpu";
import "drivers/pl011";

@extern fn dprintk(format: uint8*, ...);
@extern fn io_register_module(name: uint8*, drv_info: uint64,
                              read: fn (uint8*, uint64, uint64, uint64) -> int32,
                              write: fn (uint8*, uint64, uint64, uint64) -> int32) -> int32;

fn serial_init() {
    let mod_name: uint8* = "serial";
    dprintk("[storage] adding \"/dev/%s\"\r\n", mod_name);
    io_register_module(mod_name, 0, serial_read, serial_write);
}

fn serial_read(buffer: uint8*, count: uint64, offset: uint64, drv_info: uint64) -> int32 {
    let c: uint8;
    let i: uint64 = 0;
    while (i < count) {
        // _wfi_while(pl011_getc(&c));
        while (pl011_getc(&c))
            wfi();
        buffer[i] = c;
        i = i + 1;
    }
    return count as int32;
}

fn serial_write(buffer: uint8*, count: uint64, offset: uint64, drv_info: uint64) -> int32 {
    let i: uint64 = 0;
    while (i < count) {
        pl011_putc(buffer[i]);
        i = i + 1;
    }
    return count as int32;
}