#include "init.h"
#include <arch/syscall.h>
#include <debug.h>
#include <drivers/pl011.h>
#include <drivers/virtio_mmio.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void init() {
    // initialize block devices
    virtio_slot_t slot = -1;

    while ((slot = virtio_mmio_find_next_slot(VIRTIO_DEVICE_ID_BLOCK, slot)) != -1) {
        // build path, will eventually replace with a direct fetch
        char path[50] = {0};
        sprintf(path, "/dev/sd%d", slot);

        printk("[init] mounting block device \"%s\"\r\n", path);

        // mount volume
        int status = fat32_mount(path);
        if (status < 0) {
            printk("[init] fat32_mount() returned %d!\r\n", status);
            continue;
        }

        printk("[init] block device \"%s\" mounted!\r\n", path);
    }

    // init console
    console();

    syscall_exit(0);
}

void console() {
    char *pathname = "/dev/serial";

    while (1) {
        vfs_write(pathname, (uint8_t *)"> ", 2, 0);

        char buffer[1024] = {0};
        int len = getline(pathname, buffer);

        if (strcmp(buffer, "ls") == 0)
            vfs_dump_fs();
        else
            printk("buffer=\"%s\", len=%d\r\n", buffer, len);
    }
}

char getc(char *pathname) {
    int8_t _escape = 0, _arrow = 0;

    while (1) {
        char c;
        vfs_read(pathname, (uint8_t *)&c, 1, 0);

        if (_escape) {
            _escape = 0;

            if (c == '[') {
                _arrow = 1;
                continue;
            }
        }

        if (_arrow) {
            _arrow = 0;
            continue;
        }

        if (c == ASCII_ESC) {
            _escape = 1; // set escape
            continue;
        }

        return c;
    }
}

int getline(char *pathname, char *buffer) {
    int n = 0;

    while (1) {
        char c = getc(pathname);

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