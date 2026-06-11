#include "init.h"
#include "console.h"
#include <arch/syscall.h>
#include <ascii.h>
#include <debug.h>
#include <drivers/virtio_mmio.h>
#include <dsa/vector.h>
#include <fs/fat32.h>
#include <fs/vfs.h>
#include <mm/heap.h>
#include <stdbool.h>
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