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
        bool _quotes = false, _backslash = false;

        char *argv[256];
        int argc = 0;

        struct vector8 vec;
        vector8_init(&vec, 10);

        for (int i = 0; i <= len; i++) {
            char c = buffer[i];
            if (_backslash) {
                _backslash = false;
                vector8_append(&vec, c);
            } else if (_quotes) {
                if (c == '\\') {
                    _backslash = true;
                } else if (c == '\"') {
                    argv[argc] = (char *)kmalloc(vec.length + 1);

                    strncpy(argv[argc], (char *)vec.data, vec.length);
                    argv[argc][vec.length] = '\0';

                    vector8_reset(&vec);

                    _quotes = false;
                    argc++;
                } else {
                    vector8_append(&vec, c);
                }
            } else {
                if (isspace(c) || c == '\0') {
                    if (vec.length > 0) {
                        argv[argc] = (char *)kmalloc(vec.length + 1);

                        strncpy(argv[argc], (char *)vec.data, vec.length);
                        argv[argc][vec.length] = '\0';

                        vector8_reset(&vec);

                        argc++;
                    }
                } else if (c == '\\') {
                    _backslash = true;
                } else if (c == '\"') {
                    _quotes = true;
                } else {
                    vector8_append(&vec, c);
                }
            }
        }

        vector8_destroy(&vec);

        printk("[console] argc=%d, argv=[", argc);
        for (int i = 0; i < argc; i++)
            printk(" \"%s\",", argv[i]);
        printk(" ]\r\n");

        if (_quotes || _backslash)
            printk("[console] invalid input!, _quotes=%d, _backslash=%d\r\n", _quotes, _backslash);
        else
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
            if (c == ASCII_HT)
                c = '\t';

            vfs_write(pathname, (uint8_t *)&c, 1, 0);
            buffer[n++] = c;
        }
    }

    return -1;
}

static int _command_exit(int argc, char *argv[]) {
    int64_t status = 0;
    if (argc > 1) {
        status = atoll(argv[1]);
    }
    printk("exiting with status %d\r\n", status);
    return status;
}

static int _command_echo(int argc, char *argv[]) {
    if (argc > 1)
        printk("%s", argv[1]);
    printk("\r\n");

    return 0;
}

static int _command_help(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[]) {
    printk("available commands:\r\n"
           "    ls              list the VFS tree\r\n"
           "    cat <path>      print a file\r\n"
           "    echo [args...]  print arguments\r\n"
           "    mount <device>  mount a FAT32 block device\r\n"
           "    exit [status]   exit with status code\r\n"
           "    help            show this message\r\n");
    return 0;
}

static int _command_cat(int argc, char *argv[]) {
    if (argc < 2) {
        printk("no file provided!\r\n");
        return -1;
    }

    size_t f_size = vfs_get_file_size(argv[1]);
    char *buffer = (char *)kmalloc(f_size + 1);

    int status = vfs_read(argv[1], (uint8_t *)buffer, f_size, 0);
    if (status < 0) {
        switch (status) {
        case VFS_IO_ERROR_FILE_NOT_FOUND:
            printk("file not found!\r\n");
            break;
        case VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND:
            printk("mountpoint not found!\r\n");
            break;
        case VFS_IO_ERROR_HANDLER_NOT_PROVIDED:
            printk("handler not provided!\r\n");
            break;
        }
        return -2;
    }

    buffer[f_size] = '\0';
    printk("%s\r\n", buffer);
    kfree(buffer);
    return 0;
}

static int _command_mount(int argc, char *argv[]) {
    if (argc < 2)
        return -1;

    int status = fat32_mount(argv[1]);
    if (status < 0) {
        printk("[mount] fat32_mount() returned %d!\r\n", status);
        return -2;
    }

    printk("[mount] block device \"%s\" mounted!\r\n", argv[1]);
    return 0;
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
        int64_t status = -1;
        if (strcmp(argv[0], "ls") == 0) {
            vfs_dump_fs();
        } else if (strcmp(argv[0], "exit") == 0) {
            status = _command_exit(argc, argv);
        } else if (strcmp(argv[0], "echo") == 0) {
            status = _command_echo(argc, argv);
        } else if (strcmp(argv[0], "cat") == 0) {
            status = _command_cat(argc, argv);
        } else if (strcmp(argv[0], "mount") == 0) {
            status = _command_mount(argc, argv);
        } else if (strcmp(argv[0], "help") == 0) {
            status = _command_help(argc, argv);
        } else {
            status = 0;
        }

        syscall_exit(status);
    }
}