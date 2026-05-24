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

int dtb_init(void *dtb_addr);
int dtb_find_prop(const char *node_path, const char *prop_name, struct fdt_prop *out);

#endif