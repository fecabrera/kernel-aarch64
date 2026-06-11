#pragma once

/**
 * pid 1 entry point. Mounts /dev/sda as a FAT32 volume (exits -1 on failure), then launches
 * console("/dev/serial"). Calls syscall_exit(0) on return.
 */
void init();