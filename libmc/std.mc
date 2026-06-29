// Ergonomic standard I/O helpers over the fd syscalls: formatted/unformatted
// writes to STDOUT_FILENO and line/char reads from STDIN_FILENO. The calling
// process must have those fds open (see libc/stdio.mc).
import "libc/string";
import "libc/stdio";
import "system/syscall";
import "string";

/**
 * Formats according to format and writes the result to standard output (no
 * trailing newline). Thin wrapper around vprintf.
 *
 * @todo: use `const format: struct string` instead.
 *
 * @param format: printf-style format string (stb_sprintf grammar; floats disabled)
 * @param ...:    variadic arguments matching the format specifiers
 */
fn print(format: char*, ...) {
    let args: va_list;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/**
 * Like print, but appends a newline after the formatted output.
 * 
 * @todo: use `const format: struct string` instead.
 *
 * @param format: printf-style format string (stb_sprintf grammar; floats disabled)
 * @param ...:    variadic arguments matching the format specifiers
 */
fn println(format: char*, ...) {
    let args: va_list;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    writechar('\n');
}

/**
 * Writes a single byte to standard output.
 *
 * @param c: byte to write
 */
@inline
fn writechar(c: char) {
    write(STDOUT_FILENO, &c, 1);
}

/**
 * Writes a string's bytes to standard output (its `length` bytes from `data`).
 *
 * @param str: string to write
 */
@inline
fn writestr(const str: slice<const char>) {
    write(STDOUT_FILENO, str.ptr, str.length);
}

/**
 * Writes a string to standard output followed by a newline.
 *
 * @param str: string to write
 */
@inline
fn writeln(const str: slice<const char>) {
    writestr(str);
    writechar('\n');
}

/**
 * Reads the next printable character from standard input, blocking until one
 * arrives. Consumes and discards ANSI escape sequences (ESC [ A/B/C/D arrow
 * keys) transparently.
 *
 * @return the next non-escape byte read
 */
fn readchar() -> char {
    let _escape = false;
    let _arrow = false;

    while (true) {
        let c: char;
        read(STDIN_FILENO, &c, 1);

        if (_escape) {
            _escape = false;

            if (c == '[') {
                _arrow = true;
                continue;
            }
        }

        if (_arrow) {
            _arrow = false;
            continue;
        }

        if (c == '\e') { // ASCII_ESC
            _escape = true; // set escape
            continue;
        }

        return c;
    }
    
    return 0;
}

/**
 * Reads one line from standard input into buffer, echoing each character back.
 * Returns on CR (echoing a newline); DEL erases the last character (echoing a
 * backspace); other characters are appended. The buffer is null-terminated, and
 * the caller must ensure it is large enough for the line.
 *
 * @todo: use `const str: struct buffer` instead..
 *
 * @param buffer: output buffer for the line (null-terminated on return)
 *
 * @return number of characters in the line, excluding the null terminator
 */
fn readline(buffer: char*) -> uint64 {
    let count: uint64 = 0;

    while (true) {
        let c: char = readchar();

        case (c) {
        when '\r': // ASCII_CR
            print("\n");
            buffer[count] = '\0';
            return count;
        when 0x7F: // ASCII_DEL
            if (count > 0) {
                count = count - 1;
                print("\b \b");
                buffer[count] = '\0';
            }
        else:
            print("%c", c);
            buffer[count] = c;
            count = count + 1;
        }
    }

    return 0;
}
