// dtb.c
#include <string.h>
#include <dtb.h>
#include <arch/cpu.h>
#include <drivers/uart.h>

// DTB is big-endian, host is little-endian — must swap
static uint32_t be32(uint32_t x)
{
    return ((x & 0xFF000000) >> 24) | ((x & 0x00FF0000) >> 8) | ((x & 0x0000FF00) << 8) | ((x & 0x000000FF) << 24);
}

static const struct fdt_header *hdr;
static const char *strings; // string table base
static const uint32_t *struct_base;

extern void *dtb_ptr;

int dtb_init()
{
    hdr = (const struct fdt_header *)dtb_ptr;

    if (be32(hdr->magic) != FDT_MAGIC)
    {
        uart_puts("[dtb] bad magic!\r\n");
        return -1;
    }

    strings = (const char *)dtb_ptr + be32(hdr->off_dt_strings);
    struct_base = (const uint32_t *)((uint8_t *)dtb_ptr + be32(hdr->off_dt_struct));

    struct fdt_prop prop;
    if (dtb_find_prop("", "model", &prop) == 0)
    {
        uart_puts("[dtb] model = ");
        uart_puts((const char *)prop.data);
        uart_puts("\r\n");
    }
    if (dtb_find_prop("", "compatible", &prop) == 0)
    {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        uart_puts("[dtb] compatible =");
        while (s < end)
        {
            uart_puts(" \"");
            uart_puts(s);
            uart_puts("\"");
            s += strlen(s) + 1;
        }
        uart_puts("\r\n");
    }

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

void dtb_dump()
{
    uart_puts("\r\n=== dtb dump ===\r\n");

    const uint32_t *p = struct_base;
    int depth = 0;

    while (1)
    {
        uint32_t token = be32(*p++);

        if (token == FDT_BEGIN_NODE)
        {
            const char *name = (const char *)p;
            for (int i = 0; i < depth; i++)
                uart_puts("  ");
            uart_puts("[node] ");
            uart_puts(*name ? name : "/");
            uart_puts("\r\n");
            depth++;
            uint32_t len = strlen(name) + 1;
            p += (len + 3) / 4;
        }
        else if (token == FDT_END_NODE)
        {
            depth--;
        }
        else if (token == FDT_PROP)
        {
            uint32_t len = be32(*p++);
            uint32_t nameoff = be32(*p++);
            for (int i = 0; i < depth; i++)
                uart_printf("  ");
            uart_printf("  %s ( %i bytes)\r\n", strings + nameoff, len);
            p += (len + 3) / 4;
        }
        else if (token == FDT_END)
        {
            break;
        }
    }
    uart_puts("=================\r\n\r\n");
}

int dtb_get_memory_register(struct memreg *ptr)
{
    struct fdt_prop prop;
    int ret = dtb_find_prop("memory@40000000", "reg", &prop);
    if (ret == 0)
    {
        const uint32_t *cells = (const uint32_t *)prop.data;
        ptr->base = (uint64_t)(be32(cells[0]) << 32) | be32(cells[1]);
        ptr->size = (uint64_t)(be32(cells[2]) << 32) | be32(cells[3]);
    }
    return ret;
}

uint32_t dtb_get_irq_number(const struct fdt_prop *prop, uint32_t index)
{
    const uint32_t *cells = (const uint32_t *)prop->data + index * 3;
    uint32_t type = be32(cells[0]);   // 0=SPI, 1=PPI
    uint32_t number = be32(cells[1]); // interrupt number
    // uint32_t flags = be32(cells[2]); // trigger type (edge/level)

    if (type == DT_IRQ_TYPE_SPI)
        return GIC_SPI_BASE + number;
    if (type == DT_IRQ_TYPE_PPI)
        return GIC_PPI_BASE + number;

    return number;
}

uint32_t dtb_get_irq_count(const struct fdt_prop *prop)
{
    return prop->len / (3 * sizeof(uint32_t));
}

int dtb_get_uart_irq_number(uint32_t *ptr)
{
    struct fdt_prop prop;
    int ret = dtb_find_prop("pl011@9000000", "interrupts", &prop);
    if (ret == 0)
    {
        uint32_t n = dtb_get_irq_count(&prop);
        uart_printf("[dtb] Discovered %i UART devices", n);

        *ptr = dtb_get_irq_number(&prop, 0);
    }
    if (dtb_find_prop("pl011@90000000", "compatible", &prop) == 0)
    {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        uart_printf(", compatible =");
        while (s < end)
        {
            uart_printf(" \"%s\"", s);
            s += strlen(s) + 1;
        }
    }
    uart_printf("\r\n");
    return ret;
}

int dtb_get_timer_irq_number(uint32_t *ptr)
{
    struct fdt_prop prop;
    int ret = dtb_find_prop("timer", "interrupts", &prop);
    if (ret == 0)
    {
        uint32_t n = dtb_get_irq_count(&prop);
        uart_printf("[dtb] Discovered %i timer devices", n);

        *ptr = dtb_get_irq_number(&prop, 1); // entry 1 = EL1 physical timer
    }
    if (dtb_find_prop("timer", "compatible", &prop) == 0)
    {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        uart_printf(", compatible =");
        while (s < end)
        {
            uart_printf(" \"%s\"", s);
            s += strlen(s) + 1;
        }
    }
    uart_printf("\r\n");
    return ret;
}

int dtb_get_rtc_irq_number(uint32_t *ptr)
{
    struct fdt_prop prop;
    int ret = dtb_find_prop("pl031@9010000", "interrupts", &prop);
    if (ret == 0)
    {
        uint32_t n = dtb_get_irq_count(&prop);
        uart_printf("[dtb] Discovered %i RTC devices", n);

        *ptr = dtb_get_irq_number(&prop, 0);
    }
    if (dtb_find_prop("pl031@9010000", "compatible", &prop) == 0)
    {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        uart_printf(", compatible =");
        while (s < end)
        {
            uart_printf(" \"%s\"", s);
            s += strlen(s) + 1;
        }
    }
    uart_printf("\r\n");
    return ret;
}
