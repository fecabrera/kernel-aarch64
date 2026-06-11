#include "console.h"
#include <arch/syscall.h>
#include <ascii.h>
#include <ctype.h>
#include <debug.h>
#include <drivers/virtio_mmio.h>
#include <dsa/vector.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void console(char *pathname) {
    while (true) {
        vfs_write(pathname, (uint8_t *)"> ", 2, 0);

        char buffer[1024] = {0};
        int len = console_getline(pathname, buffer);

        char *argv[256];
        int argc = 0;

        struct vector8 vec;
        vector8_init(&vec, 10);

        for (int i = 0; i <= len; i++) {
            char c = buffer[i];

            if (isspace(c) || c == '\0') {
                if (vec.length > 0) {
                    argv[argc] = (char *)kmalloc(vec.length + 1);

                    strncpy(argv[argc], (char *)vec.data, vec.length);
                    argv[argc][vec.length] = '\0';

                    vector8_reset(&vec);

                    argc++;
                }

                continue;
            }

            vector8_append(&vec, c);
        }

        vector8_destroy(&vec);

        printk("[console] argc=%d, argv=[", argc);
        for (int i = 0; i < argc; i++)
            printk(" \"%s\",", argv[i]);
        printk(" ]\r\n");

        console_parse_command(argc, argv);

        for (int i = 0; i < argc; i++)
            kfree(argv[i]);
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

    while (true) {
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

static void _command_exit(int argc, char *argv[]) {
    int64_t status = 0;
    if (argc > 1) {
        status = atoll(argv[1]);
    }
    printk("exiting with status %d\r\n", status);
    syscall_exit(status);
}

void console_parse_command(int argc, char *argv[]) {
    if (argc == 0)
        return;

    pid_t pid = syscall_fork();
    if (pid < 0) {
        printk("[console] fork() returned %d!\r\n", printk);
        return;
    }

    if (pid > 0) {
        int64_t status = syscall_waitpid(pid);
        printk("[console] process %d returned %d!\r\n", pid, status);
        return;
    } else {
        if (strcmp(argv[0], "ls") == 0) {
            vfs_dump_fs();
        } else if (strcmp(argv[0], "exit") == 0) {
            _command_exit(argc, argv);
        } else {
            syscall_exit(0);
        }

        syscall_exit(-1);
    }
}