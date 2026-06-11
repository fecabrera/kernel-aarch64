#pragma once

/**
 * pid 1 entry point. Mounts all FAT32 block devices found via virtio MMIO, dumps the VFS tree,
 * reads a test file, then launches console(). Calls syscall_exit(0) on return.
 */
void init();

/**
 * Runs an interactive console loop on /dev/serial. Prompts with "> " and reads one line at a time
 * via read_line. Does not return.
 */
void console();

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
int read_line(char *pathname, char *buffer);
