// dtb.h
#ifndef DTB_H
#define DTB_H

#include <types.h>

// DTB header — all fields big-endian
struct fdt_header
{
    uint32_t magic; // 0xD00DFEED
    uint32_t totalsize;
    uint32_t off_dt_struct;  // offset to structure block
    uint32_t off_dt_strings; // offset to strings block
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
};

// GIC interrupt type encoding in DT "interrupts" cells
#define DT_IRQ_TYPE_SPI 0 // Shared Peripheral Interrupt
#define DT_IRQ_TYPE_PPI 1 // Private Peripheral Interrupt

// GIC ID base offsets — DT numbers are relative, add base to get absolute GIC ID
#define GIC_SPI_BASE 32 // SPIs start at GIC ID 32
#define GIC_PPI_BASE 16 // PPIs start at GIC ID 16

#define FDT_MAGIC 0xD00DFEED
#define FDT_BEGIN_NODE 0x00000001
#define FDT_END_NODE 0x00000002
#define FDT_PROP 0x00000003
#define FDT_NOP 0x00000004
#define FDT_END 0x00000009

// Result of a property lookup
struct fdt_prop
{
    const void *data;
    uint32_t len;
};

/**
 * Initializes the DTB parser by validating the magic number and storing
 * pointers to the structure and strings blocks for subsequent lookups.
 *
 * @param dtb_addr: pointer to the DTB blob passed by the bootloader in x0
 *
 * @return 0 on success, -1 if the magic number is invalid.
 */
int dtb_init(void *dtb_addr);

/**
 * Walks the DTB structure block and prints every node name and property
 * (name and byte length) to the UART. Useful for inspecting available hardware.
 */
void dtb_dump();

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
int dtb_find_prop(const char *node_path, const char *prop_name, struct fdt_prop *out);

/**
 * Decodes an absolute GIC IRQ ID from a parsed "interrupts" property.
 * Handles SPI (type 0) and PPI (type 1) by adding the appropriate GIC base offset.
 *
 * @param prop: pointer to a property returned by dtb_find_prop for "interrupts"
 *
 * @return Absolute GIC IRQ ID.
 */
uint32_t dtb_get_irq_number(const struct fdt_prop *prop, uint32_t index);

/**
 * Returns the number of interrupt entries in an "interrupts" property.
 * Each entry is 3 cells (type, number, flags) of 4 bytes each.
 *
 * @param prop: pointer to a property returned by dtb_find_prop for "interrupts"
 *
 * @return Number of interrupt entries in the property.
 */
uint32_t dtb_get_irq_count(const struct fdt_prop *prop);

/**
 * Reads the UART0 (pl011@9000000) IRQ number from the DTB and writes it to ptr.
 *
 * @param ptr: output pointer to store the absolute GIC IRQ ID
 *
 * @return 0 on success, -1 if the node or property was not found.
 */
int dtb_get_uart_irq_number(uint32_t *ptr);

/**
 * Reads the ARM generic timer IRQ number from the DTB and writes it to ptr.
 *
 * @param ptr: output pointer to store the absolute GIC IRQ ID
 *
 * @return 0 on success, -1 if the node or property was not found.
 */
int dtb_get_timer_irq_number(uint32_t *ptr);

/**
 * Reads the RTC (pl031@9010000) IRQ number from the DTB and writes it to ptr.
 *
 * @param ptr: output pointer to store the absolute GIC IRQ ID
 *
 * @return 0 on success, -1 if the node or property was not found.
 */
int dtb_get_rtc_irq_number(uint32_t *ptr);

#endif