#pragma once

/**
 * Runs an interactive console loop on pathname. Prompts with "> ", tokenizes each line into
 * argc/argv, and dispatches to console_parse_command. Does not return.
 *
 * @param pathname: VFS path of the device to use for I/O (e.g. "/dev/serial")
 */
void console(char *pathname);

/**
 * Reads the next printable character from the VFS device at pathname, blocking until one arrives.
 * Consumes and discards ANSI escape sequences (ESC [ A/B/C/D arrow keys) transparently.
 *
 * @param pathname: VFS path of the device to read from (e.g. "/dev/serial")
 *
 * @return the next non-escape character read
 */
char console_getc(char *pathname);

/**
 * Reads one line from the VFS device at pathname into buffer, echoing each character back.
 * Handles escape sequences (arrows silently discarded), CR (echoes newline and returns), DEL
 * (echoes backspace and removes last character), and LF/tab.
 *
 * @param pathname: VFS path of the device to read from (e.g. "/dev/serial")
 * @param buffer:   output buffer for the line (not null-terminated on return)
 *
 * @return number of characters read, excluding the CR terminator
 */
int console_getline(char *pathname, char *buffer);

/**
 * Dispatches a parsed command by forking a child process. The parent waits via syscall_waitpid and
 * logs the exit status. The child runs the command and exits. Built-in commands: ls (vfs_dump_fs),
 * exit [status] (syscall_exit). Unknown commands exit silently with 0.
 *
 * @param argc: number of arguments
 * @param argv: null-terminated argument strings; argv[0] is the command name
 */
void console_parse_command(int argc, char *argv[]);