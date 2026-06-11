#include "init.h"
#include <arch/syscall.h>
#include <debug.h>
#include <drivers/pl011.h>
#include <drivers/virtio_mmio.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

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

    // dump fs
    vfs_dump_fs();

    char *f_path = "/volumes/NO NAME/README.MD";
    size_t f_size = vfs_get_file_size(f_path) + 1;
    char *f_buff = (char *)kmalloc(f_size);

    printk("[init] reading file \"%s\" (%d B)\r\n", f_path, f_size);

    int status = vfs_read(f_path, (uint8_t *)f_buff, f_size, 0);
    if (status < 0) {
        switch (status) {
        case VFS_IO_ERROR_FILE_NOT_FOUND:
            printk("[init] file \"%s\" not found!\r\n", f_path);
            break;
        default:
            printk("[init] cannot read \"%s\"!\r\n", f_path);
        }

        kfree(f_buff);
        syscall_exit(1);
    }

    printk("%s\r\n", f_buff);

    console();

    kfree(f_buff);
    syscall_exit(0);
}

void console() {
    char *pathname = "/dev/serial";

    while (1) {
        vfs_write(pathname, (uint8_t *)"> ", 2, 0);

        char buffer[1024] = {0};
        read_line(pathname, buffer);
    }
}

int read_line(char *pathname, char *buffer) {
    int _len = 0;
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

        switch (c) {
        case ASCII_ESC:
            _escape = 1; // set escape
            break;
        case ASCII_CR:
            vfs_write(pathname, (uint8_t *)"\r\n", 2, 0);
            return _len;
        case ASCII_DEL:
            if (_len > 0) {
                vfs_write(pathname, (uint8_t *)"\b \b", 3, 0);
                buffer[_len--] = '\0';
            }
            break;
        default:
            if (c == ASCII_LF)
                c = '\t';

            vfs_write(pathname, (uint8_t *)&c, 1, 0);
            buffer[_len++] = c;
        }
    }

    return -1;
}