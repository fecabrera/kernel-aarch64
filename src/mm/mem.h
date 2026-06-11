#pragma once

/**
 * Reads the RAM base address and size from the device tree (DTB) via dtb_get_memory_register and
 * logs them. Must be called after the DTB has been parsed.
 */
void mem_init();
