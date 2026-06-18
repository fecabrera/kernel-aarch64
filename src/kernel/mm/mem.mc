import "debug";
import "dtb";

@static let mem: struct memreg;

/**
 * Reads the RAM base address and size from the device tree (DTB) via dtb_get_memory_register and
 * logs them. Must be called after the DTB has been parsed.
 */
fn mem_init() {
    if (dtb_get_memory_register(&mem) == 0)
        dprintk("[mem] base: %p, size=%i MiB\n", mem.base, mem.size / (1 << 20));
    else
        dprintk("[mem] Memory register not found!");
}
