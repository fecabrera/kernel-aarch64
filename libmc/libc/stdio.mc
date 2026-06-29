import "std";
import "system/syscall";

// Bindings to the bundled stb_sprintf (src/lib/stb_sprintf.h). The build defines
// STB_SPRINTF_NOFLOAT, so the float conversions (%f/%e/%g/%a) are disabled; all
// integer/string/pointer conversions and the full flag/width/precision/length
// grammar are supported, plus stb's %b (binary) extension.

/**
 * Maximum number of characters stb_sprintf buffers before invoking a vsprintfcb
 * callback. Also the minimum size of the scratch buffer such a callback returns.
 */
const STB_SPRINTF_MIN = 512;

// Conventional file descriptors, opened on the process at startup.
const STDIN_FILENO = 0;
const STDOUT_FILENO = 1;

// Returned by character I/O on end-of-input or error.
const EOF = -1;

/**
 * Callback state for vprintf, passed through stb_sprintf's opaque `user` pointer.
 * Holds the destination fd, the running character count, and the scratch buffer
 * stb formats into before each chunk is flushed via the write syscall.
 */
struct printf_ctx {
    fd: int64;                    // destination file descriptor
    length: int32;               // running count of characters written
    tmp: char[STB_SPRINTF_MIN];  // scratch buffer stb formats into
}

/**
 * Reads the next character from standard input via readchar (blocking).
 *
 * @return the character read, as an int32
 */
fn getchar() -> int32 {
    return readchar() as int32;
}

/**
 * Writes the low byte of c to standard output via the write syscall.
 *
 * @param c: character to write (low byte used)
 *
 * @return c on success, or EOF on write failure
 */
fn putchar(c: int32) -> int32 {
    if(write(STDOUT_FILENO, &c as uint8*, 1) < 0) {
        return EOF;
    }
    return c;
}

/**
 * Formats according to format and writes the result to STDOUT_FILENO. Thin
 * variadic wrapper around vprintf.
 *
 * @param format: printf-style format string (stb_sprintf grammar; floats disabled)
 * @param ...:    variadic arguments matching the format specifiers
 *
 * @return number of characters written
 */
fn printf(format: char*, ...) -> int32 {
    let args: va_list;
    va_start(args, format);
    let n = vprintf(format, args);
    va_end(args);
    return n;
}

/**
 * Formats according to format/args and streams the result to STDOUT_FILENO via
 * the write syscall, in STB_SPRINTF_MIN chunks (vsprintfcb + vprintf_cb), without
 * a full output buffer. Requires the calling process to have fd STDOUT_FILENO open.
 *
 * @param format: printf-style format string (stb_sprintf grammar; floats disabled)
 * @param args:   variadic argument list (must be initialized by the caller)
 *
 * @return number of characters written
 */
fn vprintf(format: char*, args: va_list) -> int32 {
    let ctx: struct printf_ctx;
    ctx.fd = STDOUT_FILENO;
    ctx.length = 0;
    vsprintfcb(vprintf_cb, &ctx as uint8*, ctx.tmp, format, args);
    return ctx.length;
}

/**
 * vsprintfcb callback for vprintf. Casts c back to printf_ctx, writes the count
 * formatted bytes in buf to ctx->fd via the write syscall, accumulates the total
 * in ctx->length, and returns ctx->tmp so stb keeps formatting into the same
 * scratch buffer.
 *
 * @param buf:   chunk of formatted output to flush
 * @param c:     opaque pointer to the printf_ctx passed to vsprintfcb
 * @param count: number of valid bytes in buf
 *
 * @return ctx->tmp, the scratch buffer for stb's next chunk
 */
@private
fn vprintf_cb(buf: char*, c: uint8*, count: int32) -> char* {
    let ctx = c as struct printf_ctx*;
    write(ctx->fd, buf, count as uint64);
    ctx->length = ctx->length + count;
    return ctx->tmp;
}

/**
 * Formats into str using a printf-style format and a pre-initialized va_list.
 * Accepts the full stb_sprintf grammar — flags (-, +, space, 0, #), width and
 * precision (including * from the args), length modifiers (e.g. l/ll for 64-bit),
 * and the d/i/u/o/x/X/c/s/p/b conversions; %f/%e/%g/%a are disabled in this build.
 * Writes a null terminator but does not bound the output; prefer vsnprintf when
 * the buffer size is known.
 *
 * @param str:    output buffer
 * @param format: printf-style format string
 * @param args:   variadic argument list (must be initialized by the caller)
 *
 * @return number of characters written, not including the null terminator
 */
@extern fn vsprintf(str: char*, format: char*, args: va_list) -> int32;

/**
 * Like vsprintf, but writes at most count bytes to str (including the null
 * terminator), truncating the formatted output if it would not fit.
 *
 * @param str:    output buffer
 * @param count:  size of str in bytes, including the null terminator
 * @param format: printf-style format string
 * @param args:   variadic argument list (must be initialized by the caller)
 *
 * @return number of characters that would have been written had count been
 *         unlimited, not including the null terminator
 */
@extern fn vsnprintf(str: char*, count: int32, format: char*, args: va_list) -> int32;

/**
 * Formats into str using a printf-style format. Accepts the full stb_sprintf
 * grammar — flags (-, +, space, 0, #), width and precision (including * from the
 * args), length modifiers (e.g. l/ll for 64-bit), and the d/i/u/o/x/X/c/s/p/b
 * conversions; %f/%e/%g/%a are disabled in this build. Writes a null terminator
 * but does not bound the output; prefer snprintf when the buffer size is known.
 *
 * @param str:    output buffer
 * @param format: printf-style format string
 * @param ...:    variadic arguments matching the format specifiers
 *
 * @return number of characters written, not including the null terminator
 */
@extern fn sprintf(str: char*, format: char*, ...) -> int32;

/**
 * Like sprintf, but writes at most count bytes to str (including the null
 * terminator), truncating the formatted output if it would not fit.
 *
 * @param str:    output buffer
 * @param count:  size of str in bytes, including the null terminator
 * @param format: printf-style format string
 * @param ...:    variadic arguments matching the format specifiers
 *
 * @return number of characters that would have been written had count been
 *         unlimited, not including the null terminator
 */
@extern fn snprintf(str: char*, count: int32, format: char*, ...) -> int32;

/**
 * Streaming formatter: formats according to format/args and, every time its
 * internal buffer reaches STB_SPRINTF_MIN characters (and once more at the end),
 * invokes callback to drain the chars. callback receives the filled buffer, the
 * opaque user pointer, and the number of valid bytes, and must return a buffer of
 * at least STB_SPRINTF_MIN bytes for stb to keep writing into. Used to format
 * directly to a device — e.g. pl011_vprintf flushes each chunk to the UART —
 * without allocating a full output buffer.
 *
 * `user` is opaque to stb: it is passed through to callback unchanged, so callers
 * point it at their own context struct (carrying the scratch buffer plus any
 * state the callback needs) and cast it back inside the callback.
 *
 * @param callback: invoked per chunk; returns the next scratch buffer to use
 * @param user:     opaque pointer passed through to callback (caller-defined)
 * @param str:      initial scratch buffer stb writes into (at least STB_SPRINTF_MIN)
 * @param format:   printf-style format string
 * @param args:     variadic argument list (must be initialized by the caller)
 *
 * @return total number of characters formatted
 */
@extern fn vsprintfcb(callback: fn (char*, uint8*, int32) -> char*, user: uint8*,
                      str: char*, format: char*, args: va_list) -> int32;
