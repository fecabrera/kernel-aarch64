// dtb.c
#include <string.h>
#include "dtb.h"
#include "uart.h" // for debug prints

// DTB is big-endian, host is little-endian — must swap
static uint32_t be32(uint32_t x)
{
    return ((x & 0xFF000000) >> 24) | ((x & 0x00FF0000) >> 8) | ((x & 0x0000FF00) << 8) | ((x & 0x000000FF) << 24);
}

static const struct fdt_header *hdr;
static const char *strings; // string table base
static const uint32_t *struct_base;

int dtb_init(void *dtb_addr)
{
    hdr = (const struct fdt_header *)dtb_addr;

    if (be32(hdr->magic) != FDT_MAGIC)
    {
        uart_puts("[dtb] bad magic!\r\n");
        return -1;
    }

    strings = (const char *)dtb_addr + be32(hdr->off_dt_strings);
    struct_base = (const uint32_t *)((uint8_t *)dtb_addr + be32(hdr->off_dt_struct));
    return 0;
}

// Walk the structure block looking for node_path/prop_name
// node_path example: "pl011@9000000"  (just the node name, no leading /)
int dtb_find_prop(const char *node_path, const char *prop_name, struct fdt_prop *out)
{
    const uint32_t *p = struct_base;
    int depth = 0;
    int in_target = 0;

    while (1)
    {
        uint32_t token = be32(*p++);

        switch (token)
        {
        case FDT_BEGIN_NODE:
        {
            // Node name follows as null-terminated string
            const char *name = (const char *)p;

            // Simple string match — check if this is our node
            // (real parsers handle full paths like /soc/pl011@9000000)
            if (strncmp(name, node_path, 64) == 0)
            {
                in_target = 1;
            }

            depth++;
            // Advance past name (aligned to 4 bytes)
            uint32_t len = strlen(name) + 1;
            p += (len + 3) / 4;
            break;
        }

        case FDT_END_NODE:
            if (in_target && depth == 1)
                return -1; // prop not found
            if (in_target)
                in_target = 0;
            depth--;
            break;

        case FDT_PROP:
        {
            uint32_t prop_len = be32(*p++);
            uint32_t name_off = be32(*p++);
            const char *name = strings + name_off;
            const void *data = p;

            if (in_target && strncmp(name, prop_name, 64) == 0)
            {
                out->data = data;
                out->len = prop_len;
                return 0; // found!
            }

            // Advance past value (aligned to 4 bytes)
            p += (prop_len + 3) / 4;
            break;
        }

        case FDT_NOP:
            break;

        case FDT_END:
            return -1; // not found
        }
    }
}