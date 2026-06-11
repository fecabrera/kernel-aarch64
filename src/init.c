#include "init.h"
#include <arch/syscall.h>
#include <ascii.h>
#include <debug.h>
#include <drivers/virtio_mmio.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

void init() {
    char path[] = "/dev/sda";
    printk("[init] mounting block device \"%s\"\r\n", path);

    // mount volume
    int status = fat32_mount(path);
    if (status < 0) {
        printk("[init] fat32_mount() returned %d!\r\n", status);
        syscall_exit(-1);
    }

    printk("[init] block device \"%s\" mounted!\r\n", path);

    // init console
    console("/dev/serial");

    syscall_exit(0);
}

void console(char *pathname) {
    while (true) {
        vfs_write(pathname, (uint8_t *)"> ", 2, 0);

        char buffer[1024] = {0};
        int len = console_getline(pathname, buffer);

        if (strcmp(buffer, "ls") == 0)
            vfs_dump_fs();
        else
            printk("buffer=\"%s\", len=%d\r\n", buffer, len);
    }
}

char console_getc(char *pathname) {
    bool _escape = false, _arrow = false;

    while (true) {
        char c;
        vfs_read(pathname, (uint8_t *)&c, 1, 0);

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

        if (c == ASCII_ESC) {
            _escape = true; // set escape
            continue;
        }

        return c;
    }
}

int console_getline(char *pathname, char *buffer) {
    int n = 0;

    while (1) {
        char c = console_getc(pathname);

        switch (c) {
        case ASCII_CR:
            vfs_write(pathname, (uint8_t *)"\r\n", 2, 0);
            return n;
        case ASCII_DEL:
            if (n > 0) {
                vfs_write(pathname, (uint8_t *)"\b \b", 3, 0);
                buffer[--n] = '\0';
            }
            break;
        default:
            if (c == ASCII_LF)
                c = '\t';

            vfs_write(pathname, (uint8_t *)&c, 1, 0);
            buffer[n++] = c;
        }
    }

    return -1;
}