// GIC interrupt type encoding in DT "interrupts" cells
const DT_IRQ_TYPE_SPI = 0; // Shared Peripheral Interrupt
const DT_IRQ_TYPE_PPI = 1; // Private Peripheral Interrupt

// GIC ID base offsets — DT numbers are relative, add base to get absolute GIC ID
const GIC_SPI_BASE = 32; // SPIs start at GIC ID 32
const GIC_PPI_BASE = 16; // PPIs start at GIC ID 16

const FDT_MAGIC = 0xD00DFEED;
const FDT_BEGIN_NODE = 0x00000001;
const FDT_END_NODE = 0x00000002;
const FDT_PROP = 0x00000003;
const FDT_NOP = 0x00000004;
const FDT_END = 0x00000009;

// DTB header — all fields big-endian
struct fdt_header {
    magic: uint32; // 0xD00DFEED
    totalsize: uint32;
    off_dt_struct: uint32;  // offset to structure block
    off_dt_strings: uint32; // offset to strings block
    off_mem_rsvmap: uint32;
    version: uint32;
    last_comp_version: uint32;
    boot_cpuid_phys: uint32;
    size_dt_strings: uint32;
    size_dt_struct: uint32;
}

struct memreg {
    base: uint64;
    size: uint64;
}

// Result of a property lookup
struct fdt_prop {
    data: char*;
    length: uint32;
}

/**
 * Initializes the DTB parser by validating the magic number and storing pointers to the structure
 * and strings blocks for subsequent lookups.
 *
 * @return 0 on success, -1 if the magic number is invalid.
 */
@extern fn dtb_init() -> int32;

/**
 * Walks the DTB structure block and prints every node name and property (name and byte length) to
 * the UART. Useful for inspecting available hardware.
 */
@extern fn dtb_dump();

/**
 * Walks the DTB structure block searching for a property within a named node.
 * Matches node_path against the bare node name (e.g. "pl011@9000000"), not a full path.
 *
 * @param node_path: node name to search for
 * @param prop_name: property name to find within that node
 * @param out: output struct populated with a pointer to the property data and its length
 *
 * @return 0 if the property was found, -1 otherwise.
 */
@extern fn dtb_find_prop(node_path: char*, prop_name: char*, out: struct fdt_prop*) -> int32;

/**
 * Reads the base address and size of the main RAM region from the DTB memory@40000000 node's reg
 * property and writes them to ptr.
 *
 * @param ptr: output struct populated with the physical base address and size in bytes
 *
 * @return 0 on success, -1 if the node or property was not found.
 */
@extern fn dtb_get_memory_register(ptr: struct memreg*) -> int32;

/**
 * Decodes an absolute GIC IRQ ID from a parsed "interrupts" property.
 * Handles SPI (type 0) and PPI (type 1) by adding the appropriate GIC base offset.
 *
 * @param prop:  pointer to a property returned by dtb_find_prop for "interrupts"
 * @param index: zero-based index of the interrupt entry to decode
 *
 * @return Absolute GIC IRQ ID.
 */
@extern fn dtb_get_irq_number(prop: struct fdt_prop*, index: uint32) -> uint32;

/**
 * Returns the number of interrupt entries in an "interrupts" property.
 * Each entry is 3 cells (type, number, flags) of 4 bytes each.
 *
 * @param prop: pointer to a property returned by dtb_find_prop for "interrupts"
 *
 * @return Number of interrupt entries in the property.
 */
@extern fn dtb_get_irq_count(prop: struct fdt_prop*) -> uint32;

/**
 * Reads the UART0 (pl011@9000000) IRQ number from the DTB and writes it to ptr.
 *
 * @param ptr: output pointer to store the absolute GIC IRQ ID
 *
 * @return 0 on success, -1 if the node or property was not found.
 */
@extern fn dtb_get_pl011_irq_number(ptr: uint32*) -> int32;

/**
 * Reads the ARM generic timer IRQ number from the DTB and writes it to ptr.
 *
 * @param ptr: output pointer to store the absolute GIC IRQ ID
 *
 * @return 0 on success, -1 if the node or property was not found.
 */
@extern fn dtb_get_timer_irq_number(ptr: uint32*) -> int32;

/**
 * Reads the RTC (pl031@9010000) IRQ number from the DTB and writes it to ptr.
 *
 * @param ptr: output pointer to store the absolute GIC IRQ ID
 *
 * @return 0 on success, -1 if the node or property was not found.
 */
@extern fn dtb_get_rtc_irq_number(ptr: uint32*) -> int32;

/**
 * Reads the absolute GIC IRQ ID for the nth virtio MMIO device from the DTB. virtio MMIO devices
 * are enumerated in address order (virtio_mmio@a000000 is n=0, virtio_mmio@a000200 is n=1, etc.).
 *
 * @param n:   zero-based index of the virtio MMIO device
 * @param ptr: output pointer to store the absolute GIC IRQ ID
 *
 * @return 0 on success, -1 if the node or property was not found.
 */
@extern fn dtb_get_virtio_mmio_irq_number(n: int32, ptr: uint32*) -> int32;
