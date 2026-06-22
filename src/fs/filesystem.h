#pragma once

#include <stdint.h>
#include <string.h>

typedef int (*fs_handler_t)(char *, uint8_t *, size_t);

struct fs_node {
    char *name;
    size_t file_size;
    uint16_t attrs;
    void *info;  // driver-private pointer (e.g. fat32_entry_reference *)
    void *mount; // vfs_mount * for this node's covering mountpoint, or NULL
    struct fs_node *next;
    struct fs_node *child;
};