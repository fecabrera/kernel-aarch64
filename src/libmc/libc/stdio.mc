// Bindings to the bundled stb_sprintf (src/lib/stb_sprintf.h). The build defines
// STB_SPRINTF_NOFLOAT, so the float conversions (%f/%e/%g/%a) are disabled; all
// integer/string/pointer conversions and the full flag/width/precision/length
// grammar are supported, plus stb's %b (binary) extension.

/**
 * Maximum number of characters stb_sprintf buffers before invoking a vsprintfcb
 * callback. Also the minimum size of the scratch buffer such a callback returns.
 */
const STB_SPRINTF_MIN = 512;

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
@extern fn vsprintf(str: uint8*, format: uint8*, args: va_list) -> int32;

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
@extern fn vsnprintf(str: uint8*, count: int32, format: uint8*, args: va_list) -> int32;

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
@extern fn sprintf(str: uint8*, format: uint8*, ...) -> int32;

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
@extern fn snprintf(str: uint8*, count: int32, format: uint8*, ...) -> int32;

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
@extern fn vsprintfcb(callback: fn (uint8*, uint8*, int32) -> uint8*, user: uint8*,
                      str: uint8*, format: uint8*, args: va_list) -> int32;
