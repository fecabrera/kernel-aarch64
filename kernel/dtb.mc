import "std";
import "fdt";
import "memory";
import "endian";

// GIC interrupt type encoding in DT "interrupts" cells
const DT_IRQ_TYPE_SPI = 0; // Shared Peripheral Interrupt
const DT_IRQ_TYPE_PPI = 1; // Private Peripheral Interrupt

// GIC ID base offsets — DT numbers are relative, add base to get absolute GIC ID
const GIC_SPI_BASE = 32; // SPIs start at GIC ID 32
const GIC_PPI_BASE = 16; // PPIs start at GIC ID 16

// GIC interrupt type encoding in DT "interrupts" cells
enum dt_irq_type: uint32 {
    SPI = 0, // Shared Peripheral Interrupt
    PPI = 1, // Private Peripheral Interrupt
}

struct irq_prop {
    type: dt_irq_type;
    number: uint32;
}

struct reg_prop {
    base: uint64;
    size: uint64;
}

@static let dtb_file: fdt_file;

@extern let dtb_ptr: byte*;

/**
 * Initializes the DTB parser from the boot-supplied dtb_ptr: validates the header and builds an
 * in-memory node/property tree for subsequent lookups. Must be called after kheap_init (the tree is
 * heap-allocated) and before any other dtb_* call.
 */
fn dtb_init() {
    fdt_file_init(&dtb_file, dtb_ptr);
    fdt_file_build_tree(&dtb_file);
}

/**
 * Walks the parsed device tree and prints every node and property to the kernel log. Useful for
 * inspecting available hardware.
 */
fn dtb_dump() {
    fdt_file_dump_tree(&dtb_file);
}

/**
 * Resolves an absolute GIC IRQ ID from a node's "interrupts"-style property. Looks up the node by
 * full path, decodes the interrupt entry at index (SPI adds GIC_SPI_BASE, PPI adds GIC_PPI_BASE),
 * and writes the result to out.
 *
 * @param path:      full node path, e.g. "/pl011@9000000" or "/timer"
 * @param prop_name: interrupt property name, e.g. "interrupts"
 * @param index:     zero-based index of the interrupt entry to decode
 * @param out:       output pointer for the absolute GIC IRQ ID
 *
 * @return true on success; false if the node, property, or entry was not found.
 */
fn dtb_find_irq_number(path: char*, prop_name: char*, index: uint32, out: uint32*) -> bool {
    let node = dtb_find_node(path);
    if (node == null) {
        dprintk("[dtb] node \"%s\" not found\n", path);
        return false;
    }
    
    let prop = dtb_node_find_prop(node, prop_name);
    if (prop == null) {
        dprintk("[dtb] prop \"%s\" not found\n", prop_name);
        return false;
    }

    let status = dtb_prop_get_irq_number(prop, index, out);
    if (status < 0) {
        dprintk("[dtb] dtb_prop_get_irq_number() returned %d\n", status);
        return false;
    }

    return true;
}

/**
 * Reads a base/size pair from a node's "reg"-style property. Looks up the node by full path, copies
 * the two big-endian cells, and byte-swaps them into out.
 *
 * @param path:      full node path, e.g. "/memory@40000000"
 * @param prop_name: register property name, e.g. "reg"
 * @param out:       output struct populated with the physical base address and size in bytes
 *
 * @return true on success; false if the node or property was not found.
 */
fn dtb_find_memory_register(path: char*, prop_name: char*, out: reg_prop*) -> bool {
    let node = dtb_find_node(path);
    if (node == null) {
        dprintk("[dtb] node \"%s\" not found\n", path);
        return false;
    }
    
    let prop = dtb_node_find_prop(node, prop_name);
    if (prop == null) {
        dprintk("[dtb] prop \"%s\" not found\n", prop_name);
        return false;
    }
    
    let mem = prop->dt_prop->value as reg_prop*;
    bytecopy(out, mem, 1);
    
    out->base = be64(out->base);
    out->size = be64(out->size);

    return true;
}

/**
 * Decodes the interrupt entry at index from a parsed "interrupts" property into an absolute GIC IRQ
 * ID (SPI + GIC_SPI_BASE, PPI + GIC_PPI_BASE), written to out.
 *
 * @return 0 on success, -1 if prop is null, -2 if index is out of range, -3 for an unknown type.
 */
@private
fn dtb_prop_get_irq_number(prop: fdt_tree_prop*, index: uint32, out: uint32*) -> int32 {
    if (prop == null) return -1;

    if (index >= be32(prop->dt_prop->length) / sizeof(irq_prop)) return -2;
    
    let irqs = prop->dt_prop->value as irq_prop*;
    let irq = irqs[index];

    case (be32(irq.type)) {
    when dt_irq_type::SPI:
        *out = GIC_SPI_BASE + be32(irq.number);
        return 0;
    when DT_IRQ_TYPE_PPI:
        *out = GIC_PPI_BASE + be32(irq.number);
        return 0;
    }

    return -3;
}

/**
 * Looks up a node in the parsed device tree by full path (e.g. "/pl011@9000000").
 *
 * @return the matching node, or null if no node has that path.
 */
@private @inline fn dtb_find_node(path: char*) -> fdt_tree_node* {
    return fdt_file_find_node(&dtb_file, path);
}

/**
 * Looks up a property by name within a node, resolving the property's name offset against the DTB
 * string block.
 *
 * @return the matching property, or null if the node has no property with that name.
 */
@private @inline fn dtb_node_find_prop(node: fdt_tree_node*, name: char*) -> fdt_tree_prop* {
    return fdt_tree_node_find_prop(node, name, dtb_file.dt_strings);
}
