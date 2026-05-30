#include <debug.h>
#include <dtb.h>
#include <string.h>
#include "gic.h"
#include "virtio_mmio.h"

static int _irq_to_slot[256] = {0};
static virtio_mmio_handler_t _handlers[32];

static struct virtq_desc desc;
static struct virtq_avail avail;
static struct virtq_used used;

static int _virtio_mmio_init(int i)
{
    uint32_t magic = VIRTIO_MAGIC(i);
    uint32_t version = VIRTIO_VERSION(i);
    uint32_t device_id = VIRTIO_DEVICE_ID(i);

    // parse magic
    if (magic != VIRTIO_MMIO_MAGIC)
        return VIRTIO_MMIO_BAD_MAGIC;

    // parse version
    if (version != VIRTIO_MMIO_VERSION_MODERN)
        return VIRTIO_MMIO_UNSUPPORTED_VERSION;

    // parse device ID
    if (device_id == VIRTIO_DEVICE_ID_INVALID)
        return VIRTIO_MMIO_INVALID_DEVICE;

    dprintk("[virtio_mmio@%x] magic = 0x%x, version = %i, device_id = %i\r\n", VIRTIO_MMIO_ADDR(i), magic, version, VIRTIO_DEVICE_ID(i));

    // acknowledge
    VIRTIO_STATUS(i) = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

    // get features
    uint32_t features_lo, features_hi;

    VIRTIO_DEVICE_FEATURES_SEL(i) = VIRTIO_FEATURES_LOW;
    features_lo = VIRTIO_DEVICE_FEATURES(i);

    VIRTIO_DEVICE_FEATURES_SEL(i) = VIRTIO_FEATURES_HIGH;
    features_hi = VIRTIO_DEVICE_FEATURES(i);

    dprintk("[virtio_mmio@%x] features: low=0x%x, high=0x%x\r\n", VIRTIO_MMIO_ADDR(i), features_lo, features_hi);

    // negociate features
    dprintk("[virtio_mmio@%x] negociating features\r\n", VIRTIO_MMIO_ADDR(i));

    VIRTIO_DRIVER_FEATURES_SEL(i) = VIRTIO_FEATURES_LOW;
    VIRTIO_DRIVER_FEATURES(i) = features_lo & (VIRTIO_BLK_F_BLK_SIZE | VIRTIO_BLK_F_SIZE_MAX);

    VIRTIO_DRIVER_FEATURES_SEL(i) = VIRTIO_FEATURES_HIGH;
    VIRTIO_DRIVER_FEATURES(i) = features_hi & VIRTIO_F_VERSION_1; // must keep this

    VIRTIO_STATUS(i) |= VIRTIO_STATUS_FEATURES_OK;

    // check response
    if (!(VIRTIO_STATUS(i) & VIRTIO_STATUS_FEATURES_OK))
    {
        dprintk("[virtio_mmio@%x] features rejected\r\n", VIRTIO_MMIO_ADDR(i));
        VIRTIO_STATUS(i) |= VIRTIO_STATUS_FAILED;

        return VIRTIO_MMIO_NEGOTIATION_FAILED; // negotiation failed
    }

    // negotiation accepted
    dprintk("[virtio_mmio@%x] features accepted\r\n", VIRTIO_MMIO_ADDR(i));

    // set up virtqueue
    VIRTIO_QUEUE_SEL(i) = 0;

    uint32_t virtio_queue_num_max = VIRTIO_QUEUE_NUM_MAX(i);
    dprintk("[virtio_mmio@%x] virtio_queue_num_max=%i\r\n", VIRTIO_MMIO_ADDR(i), virtio_queue_num_max);

    if (virtio_queue_num_max < DEFAULT_VIRTIO_QUEUE_NUM)
    {
        dprintk("[virtio_mmio@%x] device doesn't support queue_num=%i\r\n", VIRTIO_MMIO_ADDR(i), DEFAULT_VIRTIO_QUEUE_NUM);
        return VIRTIO_MMIO_QUEUE_NUM_ERROR;
    }

    VIRTIO_QUEUE_NUM(i) = DEFAULT_VIRTIO_QUEUE_NUM;

    // Write physical addresses (split into low/high 32-bit halves)
    VIRTIO_QUEUE_DESC_LOW(i) = (uint32_t)((uintptr_t)&desc);
    VIRTIO_QUEUE_DESC_HIGH(i) = (uint32_t)((uintptr_t)&desc >> 32);

    VIRTIO_QUEUE_DRIVER_LOW(i) = (uint32_t)((uintptr_t)&avail);
    VIRTIO_QUEUE_DRIVER_HIGH(i) = (uint32_t)((uintptr_t)&avail >> 32);

    VIRTIO_QUEUE_DEVICE_LOW(i) = (uint32_t)((uintptr_t)&used);
    VIRTIO_QUEUE_DEVICE_HIGH(i) = (uint32_t)((uintptr_t)&used >> 32);

    // Mark the queue live
    VIRTIO_QUEUE_READY(i) = 1;

    // initialize IRQ
    uint32_t virtio_mmio_irq;
    if (dtb_get_virtio_mmio_irq_number(i, &virtio_mmio_irq) != 0)
    {
        dprintk("[virtio_mmio] IRQ not found!!\r\n");
        return VIRTIO_MMIO_IRQ_NOT_FOUND;
    }

    _irq_to_slot[virtio_mmio_irq] = i;

    dprintk("[virtio_mmio@%x] Initializing IRQ: %i\r\n", VIRTIO_MMIO_ADDR(i), virtio_mmio_irq);
    irq_register_handler(virtio_mmio_irq, &virtio_mmio_irq_handler);
    gic_enable_irq(virtio_mmio_irq);

    return 0;
}

void virtio_mmio_init()
{
    for (int i = 0; i < 32; i++)
        _virtio_mmio_init(i);
}

struct cpu_context *virtio_mmio_irq_handler(int irq, struct cpu_context *ctx)
{
    int i = _irq_to_slot[irq];
    virtio_mmio_handler_t fnc = _handlers[i];

    printk("[virtio_mmio@%x] IRQ handler\r\n", VIRTIO_MMIO_ADDR(i));

    if (fnc == NULL)
    {
        dprintk("[virtio_mmio@%x] Handler not found for slot %i!", VIRTIO_MMIO_ADDR(i), i);
        return ctx;
    }

    return fnc(i, ctx);
}
