#include <arch/cpu.h>
#include <debug.h>
#include <drivers/virtio_mmio.h>
#include <dtb.h>
#include <stdio.h>
#include <string.h>

static const struct fdt_header *hdr;
static const char *strings; // string table base
static const uint32_t *struct_base;

extern void *dtb_ptr;

int dtb_init() {
    hdr = (const struct fdt_header *)dtb_ptr;

    if (_be32(hdr->magic) != FDT_MAGIC) {
        dprintk("[dtb] bad magic!\r\n");
        return -1;
    }

    strings = (const char *)dtb_ptr + _be32(hdr->off_dt_strings);
    struct_base = (const uint32_t *)((uint8_t *)dtb_ptr + _be32(hdr->off_dt_struct));

    struct fdt_prop prop;
    if (dtb_find_prop("", "model", &prop) == 0) {
        dprintk("[dtb] model = ");
        dprintk((const char *)prop.data);
        dprintk("\r\n");
    }
    if (dtb_find_prop("", "compatible", &prop) == 0) {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        dprintk("[dtb] compatible =");
        while (s < end) {
            dprintk(" \"");
            dprintk(s);
            dprintk("\"");
            s += strlen(s) + 1;
        }
        dprintk("\r\n");
    }

    return 0;
}

// Walk the structure block looking for node_path/prop_name
// node_path example: "pl011@9000000"  (just the node name, no leading /)
int dtb_find_prop(const char *node_path, const char *prop_name, struct fdt_prop *out) {
    const uint32_t *p = struct_base;
    int depth = 0;
    int in_target = 0;

    while (1) {
        uint32_t token = _be32(*p++);

        switch (token) {
        case FDT_BEGIN_NODE: {
            // Node name follows as null-terminated string
            const char *name = (const char *)p;

            // Simple string match — check if this is our node
            // (real parsers handle full paths like /soc/pl011@9000000)
            if (strncmp(name, node_path, 64) == 0) {
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

        case FDT_PROP: {
            uint32_t prop_len = _be32(*p++);
            uint32_t name_off = _be32(*p++);
            const char *name = strings + name_off;
            const void *data = p;

            if (in_target && strncmp(name, prop_name, 64) == 0) {
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

void dtb_dump() {
    dprintk("\r\n=== dtb dump ===\r\n");

    const uint32_t *p = struct_base;
    int depth = 0;

    while (1) {
        uint32_t token = _be32(*p++);

        if (token == FDT_BEGIN_NODE) {
            const char *name = (const char *)p;
            for (int i = 0; i < depth; i++)
                dprintk("  ");
            dprintk("[node] ");
            dprintk(*name ? name : "/");
            dprintk("\r\n");
            depth++;
            uint32_t len = strlen(name) + 1;
            p += (len + 3) / 4;
        } else if (token == FDT_END_NODE) {
            depth--;
        } else if (token == FDT_PROP) {
            uint32_t len = _be32(*p++);
            uint32_t nameoff = _be32(*p++);
            for (int i = 0; i < depth; i++)
                dprintk("  ");
            dprintk("  %s ( %i bytes)\r\n", strings + nameoff, len);
            p += (len + 3) / 4;
        } else if (token == FDT_END) {
            break;
        }
    }
    dprintk("=================\r\n\r\n");
}

int dtb_get_memory_register(struct memreg *ptr) {
    struct fdt_prop prop;
    int ret = dtb_find_prop("memory@40000000", "reg", &prop);
    if (ret == 0) {
        const uint32_t *cells = (const uint32_t *)prop.data;
        ptr->base = ((uint64_t)_be32(cells[0]) << 32) | _be32(cells[1]);
        ptr->size = ((uint64_t)_be32(cells[2]) << 32) | _be32(cells[3]);
    }
    return ret;
}

uint32_t dtb_get_irq_number(const struct fdt_prop *prop, uint32_t index) {
    const uint32_t *cells = (const uint32_t *)prop->data + index * 3;
    uint32_t type = _be32(cells[0]);   // 0=SPI, 1=PPI
    uint32_t number = _be32(cells[1]); // interrupt number
    // uint32_t flags = _be32(cells[2]); // trigger type (edge/level)

    if (type == DT_IRQ_TYPE_SPI)
        return GIC_SPI_BASE + number;
    if (type == DT_IRQ_TYPE_PPI)
        return GIC_PPI_BASE + number;

    return number;
}

uint32_t dtb_get_irq_count(const struct fdt_prop *prop) {
    return prop->len / (3 * sizeof(uint32_t));
}

int dtb_get_pl011_irq_number(uint32_t *ptr) {
    struct fdt_prop prop;
    int ret = dtb_find_prop("pl011@9000000", "interrupts", &prop);
    if (ret == 0) {
        uint32_t n = dtb_get_irq_count(&prop);
        dprintk("[dtb] Discovered %i UART devices", n);

        *ptr = dtb_get_irq_number(&prop, 0);
    }
    if (dtb_find_prop("pl011@90000000", "compatible", &prop) == 0) {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        dprintk(", compatible =");
        while (s < end) {
            dprintk(" \"%s\"", s);
            s += strlen(s) + 1;
        }
    }
    dprintk("\r\n");
    return ret;
}

int dtb_get_timer_irq_number(uint32_t *ptr) {
    struct fdt_prop prop;
    int ret = dtb_find_prop("timer", "interrupts", &prop);
    if (ret == 0) {
        uint32_t n = dtb_get_irq_count(&prop);
        dprintk("[dtb] Discovered %i timer devices", n);

        *ptr = dtb_get_irq_number(&prop, 1); // entry 1 = EL1 physical timer
    }
    if (dtb_find_prop("timer", "compatible", &prop) == 0) {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        dprintk(", compatible =");
        while (s < end) {
            dprintk(" \"%s\"", s);
            s += strlen(s) + 1;
        }
    }
    dprintk("\r\n");
    return ret;
}

int dtb_get_rtc_irq_number(uint32_t *ptr) {
    struct fdt_prop prop;
    int ret = dtb_find_prop("pl031@9010000", "interrupts", &prop);
    if (ret == 0) {
        uint32_t n = dtb_get_irq_count(&prop);
        dprintk("[dtb] Discovered %i RTC devices", n);

        *ptr = dtb_get_irq_number(&prop, 0);
    }
    if (dtb_find_prop("pl031@9010000", "compatible", &prop) == 0) {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        dprintk(", compatible =");
        while (s < end) {
            dprintk(" \"%s\"", s);
            s += strlen(s) + 1;
        }
    }
    dprintk("\r\n");
    return ret;
}

int dtb_get_virtio_mmio_irq_number(int n, uint32_t *ptr) {
    char prop_name[50];
    sprintf(prop_name, "virtio_mmio@%x", VIRTIO_MMIO_ADDR(n));

    struct fdt_prop prop;
    int ret = dtb_find_prop(prop_name, "interrupts", &prop);

    if (ret == 0) {
        uint32_t n = dtb_get_irq_count(&prop);
        dprintk("[dtb] Discovered %i virtio_mmio devices", n);

        *ptr = dtb_get_irq_number(&prop, 0);
    }
    if (dtb_find_prop(prop_name, "compatible", &prop) == 0) {
        const char *s = (const char *)prop.data;
        const char *end = s + prop.len;
        dprintk(", compatible =");
        while (s < end) {
            dprintk(" \"%s\"", s);
            s += strlen(s) + 1;
        }
    }
    dprintk("\r\n");
    return ret;
}
