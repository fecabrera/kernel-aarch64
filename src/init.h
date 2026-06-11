#pragma once

/**
 * pid 1 entry point. Mounts all FAT32 block devices found via virtio MMIO, then launches
 * console(). Calls syscall_exit(0) on return.
 */
void init();

/**
 * Runs an interactive console loop on /dev/serial. Prompts with "> ", reads one line at a time via
 * console_getline, and dispatches built-in commands (ls: vfs_dump_fs). Does not return.
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
