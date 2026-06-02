#include <debug.h>
#include <dtb.h>
#include <string.h>
#include <arch/syscall.h>
#include "gic.h"
#include "virtio_mmio.h"

static int _irq_to_slot[256] = {0};
static virtio_mmio_handler_t _handlers[32];
static struct virtio_mmio_device _devices[32] = {0};

static int _virtio_mmio_probe_device(int slot)
{
    struct virtio_mmio_device *device = &_devices[slot];

    uint32_t magic = VIRTIO_MAGIC(slot);
    uint32_t version = VIRTIO_VERSION(slot);
    uint32_t device_id = VIRTIO_DEVICE_ID(slot);

    // parse magic
    if (magic != VIRTIO_MMIO_MAGIC)
        return VIRTIO_MMIO_BAD_MAGIC;

    // parse version
    if (version != VIRTIO_MMIO_VERSION_MODERN)
        return VIRTIO_MMIO_UNSUPPORTED_VERSION;

    // parse device ID
    if (device_id == VIRTIO_DEVICE_ID_INVALID)
        return VIRTIO_MMIO_INVALID_DEVICE;

    dprintk("[virtio_mmio@%x] magic = 0x%x, version = %i, device_id = %i\r\n", VIRTIO_MMIO_ADDR(slot), magic, version, VIRTIO_DEVICE_ID(slot));

    // acknowledge
    VIRTIO_STATUS(slot) = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

    // get features
    uint32_t features_lo, features_hi;

    VIRTIO_DEVICE_FEATURES_SEL(slot) = VIRTIO_FEATURES_LOW;
    features_lo = VIRTIO_DEVICE_FEATURES(slot);

    VIRTIO_DEVICE_FEATURES_SEL(slot) = VIRTIO_FEATURES_HIGH;
    features_hi = VIRTIO_DEVICE_FEATURES(slot);

    dprintk("[virtio_mmio@%x] features: low=0x%x, high=0x%x\r\n", VIRTIO_MMIO_ADDR(slot), features_lo, features_hi);

    // negociate features
    dprintk("[virtio_mmio@%x] negociating features\r\n", VIRTIO_MMIO_ADDR(slot));

    VIRTIO_DRIVER_FEATURES_SEL(slot) = VIRTIO_FEATURES_LOW;
    VIRTIO_DRIVER_FEATURES(slot) = features_lo & (VIRTIO_BLK_F_BLK_SIZE | VIRTIO_BLK_F_SIZE_MAX);

    VIRTIO_DRIVER_FEATURES_SEL(slot) = VIRTIO_FEATURES_HIGH;
    VIRTIO_DRIVER_FEATURES(slot) = features_hi & VIRTIO_F_VERSION_1; // must keep this

    VIRTIO_STATUS(slot) |= VIRTIO_STATUS_FEATURES_OK;

    // check response
    if (!(VIRTIO_STATUS(slot) & VIRTIO_STATUS_FEATURES_OK))
    {
        dprintk("[virtio_mmio@%x] features rejected\r\n", VIRTIO_MMIO_ADDR(slot));
        VIRTIO_STATUS(slot) |= VIRTIO_STATUS_FAILED;

        return VIRTIO_MMIO_NEGOTIATION_FAILED; // negotiation failed
    }

    // negotiation accepted
    dprintk("[virtio_mmio@%x] features accepted\r\n", VIRTIO_MMIO_ADDR(slot));

    // set up virtqueue
    VIRTIO_QUEUE_SEL(slot) = 0;

    uint32_t virtio_queue_num_max = VIRTIO_QUEUE_NUM_MAX(slot);
    dprintk("[virtio_mmio@%x] virtio_queue_num_max=%i\r\n", VIRTIO_MMIO_ADDR(slot), virtio_queue_num_max);

    if (virtio_queue_num_max < DEFAULT_VIRTIO_QUEUE_NUM)
    {
        dprintk("[virtio_mmio@%x] device doesn't support queue_num=%i\r\n", VIRTIO_MMIO_ADDR(slot), DEFAULT_VIRTIO_QUEUE_NUM);
        return VIRTIO_MMIO_QUEUE_NUM_ERROR;
    }

    VIRTIO_QUEUE_NUM(slot) = DEFAULT_VIRTIO_QUEUE_NUM;

    struct virtq *q = &device->queue;

    // Write physical addresses (split into low/high 32-bit halves)
    VIRTIO_QUEUE_DESC_LOW(slot) = (uint32_t)((uintptr_t)&q->desc);
    VIRTIO_QUEUE_DESC_HIGH(slot) = (uint32_t)((uintptr_t)&q->desc >> 32);

    VIRTIO_QUEUE_DRIVER_LOW(slot) = (uint32_t)((uintptr_t)&q->avail);
    VIRTIO_QUEUE_DRIVER_HIGH(slot) = (uint32_t)((uintptr_t)&q->avail >> 32);

    VIRTIO_QUEUE_DEVICE_LOW(slot) = (uint32_t)((uintptr_t)&q->used);
    VIRTIO_QUEUE_DEVICE_HIGH(slot) = (uint32_t)((uintptr_t)&q->used >> 32);

    // Mark the queue live
    VIRTIO_QUEUE_READY(slot) = 1;

    // initialize IRQ
    uint32_t virtio_mmio_irq;
    if (dtb_get_virtio_mmio_irq_number(slot, &virtio_mmio_irq) != 0)
    {
        dprintk("[virtio_mmio] IRQ not found!!\r\n");
        return VIRTIO_MMIO_IRQ_NOT_FOUND;
    }

    _irq_to_slot[virtio_mmio_irq] = slot;

    dprintk("[virtio_mmio@%x] Initializing IRQ: %i\r\n", VIRTIO_MMIO_ADDR(slot), virtio_mmio_irq);
    irq_register_handler(virtio_mmio_irq, &virtio_mmio_irq_handler);
    gic_enable_irq(virtio_mmio_irq);

    // if everything goes well we add it to our table
    device->irq_id = virtio_mmio_irq;
    device->device_id = device_id;

    return 0;
}

void virtio_mmio_init()
{
    for (int slot = 0; slot < 32; slot++)
        _virtio_mmio_probe_device(slot);
}

int virtio_mmio_find_next_slot(uint32_t device_id, int start)
{
    for (int idx = start + 1; idx < 32; idx++)
        if (_devices[idx].device_id == device_id)
            return idx;

    return -1;
}

int virtio_mmio_read(int slot, uint64_t sector_number, uint8_t *data)
{
    struct virtio_mmio_device *device = &_devices[slot];

    if (device->device_id == VIRTIO_DEVICE_ID_INVALID)
    {
        dprintk("[virtio_mmio] Device slot %i is invalid!\r\n", slot);
        return VIRTIO_MMIO_INVALID_DEVICE;
    }

    // 1. Fill the header
    struct virtio_blk_req_header hdr = {
        .type = 0,
        .sector = sector_number,
    };
    uint8_t status = 0xFF; // device writes 0 on success, 1 on error, 2 = unsupported

    struct virtq *q = &device->queue;

    // 2. Fill descriptor chain (indices 0, 1, 2)
    q->desc[0].addr = (uintptr_t)&hdr;
    q->desc[0].len = sizeof(hdr);
    q->desc[0].flags = VIRTQ_DESC_F_NEXT;
    q->desc[0].next = 1;

    q->desc[1].addr = (uintptr_t)data;
    q->desc[1].len = VIRTIO_MMIO_BLK_SECTOR_SIZE;
    q->desc[1].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
    q->desc[1].next = 2;

    q->desc[2].addr = (uintptr_t)&status;
    q->desc[2].len = 1;
    q->desc[2].flags = VIRTQ_DESC_F_WRITE;

    // 3. Post to available ring
    uint16_t idx = q->avail.idx % DEFAULT_VIRTIO_QUEUE_NUM;
    q->avail.ring[idx] = 0; // head descriptor index
    q->avail.idx++;

    // 4. Kick the device
    VIRTIO_QUEUE_NOTIFY(slot) = 0;

    // 5. Poll or wait for IRQ — device writes to used ring when done
    // In the IRQ handler, check used.idx advanced and read status byte
    while (status == VIRTIO_BLK_S_NONE)
        syscall_msleep(1);

    dprintk("[virtio_mmio@%x] status = %u\r\n", status);

    return status;
}

struct cpu_context *virtio_mmio_irq_handler(int irq, struct cpu_context *ctx)
{
    int slot = _irq_to_slot[irq];
    virtio_mmio_handler_t fnc = _handlers[slot];

    uint32_t status = VIRTIO_INTERRUPT_STATUS(slot);
    VIRTIO_INTERRUPT_ACK(slot) = status;

    dprintk("[virtio_mmio@%x] IRQ handler, status = %u\r\n", VIRTIO_MMIO_ADDR(slot), status);

    if (fnc == NULL)
    {
        dprintk("[virtio_mmio@%x] Handler not found for slot %i!\r\n", VIRTIO_MMIO_ADDR(slot), slot);
        return ctx;
    }

    return fnc(slot, ctx);
}
