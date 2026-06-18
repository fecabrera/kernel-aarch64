import "debug";
import "cpu";
import "dtb";
import "memory";
import "interrupts/irq";
import "interrupts/gic";

// ── MMIO base and per-device stride ──────────────────────────────────────────

const VIRTIO_MMIO_BASE = 0x0a000000;
const VIRTIO_MMIO_STRIDE = 0x200;

const DEFAULT_VIRTIO_QUEUE_NUM = 64;

// ── Magic / version ──────────────────────────────────────────────────────────

const VIRTIO_MMIO_MAGIC = 0x74726976;
const VIRTIO_MMIO_VERSION_LEGACY = 1;
const VIRTIO_MMIO_VERSION_MODERN = 2;

// ── Device IDs ───────────────────────────────────────────────────────────────

const VIRTIO_DEVICE_ID_INVALID = 0;
const VIRTIO_DEVICE_ID_NET = 1;
const VIRTIO_DEVICE_ID_BLOCK = 2;
const VIRTIO_DEVICE_ID_CONSOLE = 3;
const VIRTIO_DEVICE_ID_RNG = 4;
const VIRTIO_DEVICE_ID_GPU = 16;
const VIRTIO_DEVICE_ID_INPUT = 18;

// ── Status register bits (write in order during init) ────────────────────────

const VIRTIO_STATUS_ACKNOWLEDGE = (1 << 0); // driver noticed the device
const VIRTIO_STATUS_DRIVER = (1 << 1);      // driver knows how to drive it
const VIRTIO_STATUS_DRIVER_OK = (1 << 2);   // driver is ready
const VIRTIO_STATUS_FEATURES_OK = (1 << 3); // feature negotiation complete
const VIRTIO_STATUS_DEVICE_NEEDS_RESET = (1 << 6);
const VIRTIO_STATUS_FAILED = (1 << 7); // driver gave up

// ── Interrupt status / ack bits ──────────────────────────────────────────────

const VIRTIO_INT_USED_BUFFER = (1 << 0);   // device consumed a buffer
const VIRTIO_INT_CONFIG_CHANGE = (1 << 1); // device config changed

// ── Feature word selectors ───────────────────────────────────────────────────

const VIRTIO_FEATURES_LOW = 0;  // bits 0–31
const VIRTIO_FEATURES_HIGH = 1; // bits 32–63

// ── Transport feature bits (word 1, i.e. bit index relative to 32) ───────────

const VIRTIO_F_NOTIFY_ON_EMPTY = (1 << 0);    // bit 32 global
const VIRTIO_F_ANY_LAYOUT = (1 << 2);         // bit 34 global
const VIRTIO_F_RING_INDIRECT_DESC = (1 << 5); // bit 37 global
const VIRTIO_F_RING_EVENT_IDX = (1 << 6);     // bit 38 global
const VIRTIO_F_VERSION_1 = (1 << 29);         // bit 61 global — must negotiate for modern

// ── Block device feature bits (word 0) ───────────────────────────────────────

const VIRTIO_BLK_F_SIZE_MAX = (1 << 1);
const VIRTIO_BLK_F_SEG_MAX = (1 << 2);
const VIRTIO_BLK_F_GEOMETRY = (1 << 4);
const VIRTIO_BLK_F_RO = (1 << 5);
const VIRTIO_BLK_F_BLK_SIZE = (1 << 6);
const VIRTIO_BLK_F_FLUSH = (1 << 9);
const VIRTIO_BLK_F_TOPOLOGY = (1 << 10);
const VIRTIO_BLK_F_CONFIG_WCE = (1 << 11);
const VIRTIO_BLK_F_DISCARD = (1 << 13);
const VIRTIO_BLK_F_WRITE_ZEROES = (1 << 14);

//
const VIRTIO_MMIO_BLK_SECTOR_SIZE = 512;

// Block request types — written into virtio_blk_req_header.type
const VIRTIO_BLK_T_IN = 0;  // read from device into driver buffer
const VIRTIO_BLK_T_OUT = 1; // write from driver buffer to device

// Block request statuses — written by the device into the status byte of the request
const VIRTIO_BLK_S_OK = 0;      // success
const VIRTIO_BLK_S_IOERR = 1;   // device I/O error
const VIRTIO_BLK_S_UNSUPP = 2;  // unsupported request type
const VIRTIO_BLK_S_NONE = 0xFF; // sentinel: device has not yet responded

// ── Virtqueue descriptor flags ───────────────────────────────────────────────

const VIRTQ_DESC_F_NEXT = (1 << 0);     // descriptor is part of a chain; .next is valid
const VIRTQ_DESC_F_WRITE = (1 << 1);    // device writes to this buffer (driver-read-only if clear)
const VIRTQ_DESC_F_INDIRECT = (1 << 2); // buffer contains a table of descriptors

// Probing errors

const VIRTIO_MMIO_BAD_MAGIC = -1;
const VIRTIO_MMIO_UNSUPPORTED_VERSION = -2;
const VIRTIO_MMIO_INVALID_DEVICE = -3;
const VIRTIO_MMIO_NEGOTIATION_FAILED = -4;
const VIRTIO_MMIO_QUEUE_NUM_ERROR = -5;
const VIRTIO_MMIO_IRQ_NOT_FOUND = -6;

// ── Register accessors ───────────────────────────────────────────────────────
@volatile
struct virtio_mmio_regs {
    magic: uint32;               // 0x000  R   magic value (0x74726976)
    version: uint32;             // 0x004  R   device version
    device_id: uint32;           // 0x008  R   device type
    vendor_id: uint32;           // 0x00c  R   vendor
    device_features: uint32;     // 0x010  R   features offered
    device_features_sel: uint32; // 0x014  W   select feature word
    _pad0: uint32[2];            // 0x018–0x01c
    driver_features: uint32;     // 0x020  W   features accepted
    driver_features_sel: uint32; // 0x024  W   select feature word
    _pad1: uint32[2];            // 0x028–0x02c
    queue_sel: uint32;           // 0x030  W   select virtqueue
    queue_num_max: uint32;       // 0x034  R   max queue size
    queue_num: uint32;           // 0x038  W   set queue size
    _pad2: uint32[2];            // 0x03c–0x040
    queue_ready: uint32;         // 0x044  RW  activate queue
    _pad3: uint32[2];            // 0x048–0x04c
    queue_notify: uint32;        // 0x050  W   kick device
    _pad4: uint32[3];            // 0x054–0x05c
    interrupt_status: uint32;    // 0x060  R   pending interrupts
    interrupt_ack: uint32;       // 0x064  W   clear interrupts
    _pad5: uint32[2];            // 0x068–0x06c
    status: uint32;              // 0x070  RW  device status
    _pad6: uint32[3];            // 0x074–0x07c
    queue_desc_low: uint32;      // 0x080  W   descriptor table PA low
    queue_desc_high: uint32;     // 0x084  W   descriptor table PA high
    _pad7: uint32[2];            // 0x088–0x08c
    queue_driver_low: uint32;    // 0x090  W   available ring PA low
    queue_driver_high: uint32;   // 0x094  W   available ring PA high
    _pad8: uint32[2];            // 0x098–0x09c
    queue_device_low: uint32;    // 0x0a0  W   used ring PA low
    queue_device_high: uint32;   // 0x0a4  W   used ring PA high
    _pad9: uint32[21];           // 0x0a8–0x0f8
    config_gen: uint32;          // 0x0fc  R   config generation counter
    config: uint32;              // 0x100  RW  device-specific config (first word)
}

struct virtq_desc {
    addr: uint64;  // physical address of the buffer
    length: uint32;   // length in bytes
    flags: uint16; // VIRTQ_DESC_F_NEXT, VIRTQ_DESC_F_WRITE, etc.
    next: uint16;  // index of next descriptor in chain (if F_NEXT set)
}

struct virtq_avail {
    flags: uint16;                          // VIRTQ_AVAIL_F_NO_INTERRUPT
    idx: uint16;                            // next slot the driver will write
    ring: uint16[DEFAULT_VIRTIO_QUEUE_NUM]; // descriptor indices
}

struct virtq_ring {
    id: uint32;  // descriptor chain head index
    length: uint32; // bytes written (for reads)
}

struct virtq_used {
    flags: uint16; // VIRTQ_USED_F_NO_NOTIFY
    idx: uint16;   // next slot the device will write
    ring: struct virtq_ring[DEFAULT_VIRTIO_QUEUE_NUM];
}

struct virtq {
    desc: struct virtq_desc[DEFAULT_VIRTIO_QUEUE_NUM];
    avail: struct virtq_avail;
    used: struct virtq_used;
}

struct virtio_blk_req_header {
    type: uint32; // VIRTIO_BLK_T_IN (0) = read, VIRTIO_BLK_T_OUT (1) = write
    reserved: uint32;
    sector: uint64; // 512-byte sector number to start from
}

struct virtio_mmio_device {
    irq_id: uint32;
    device_id: uint32;
    queue: struct virtq;
}

@static let _irq_to_slot: int8[256];
@static let _handlers: (fn (int8, struct cpu_context*) -> struct cpu_context*)[32];
@static let _devices: struct virtio_mmio_device[32];

/**
 * Scans all 32 virtio MMIO slots, initializes each valid device found (modern version, non-zero
 * device ID), reads its feature words, and registers its IRQ handler with the GIC. Must be called
 * after gic_init and irq_init, and before irq_enable.
 */
fn virtio_mmio_init() {
    set_bytes(_irq_to_slot, 0, 256);
    set_bytes(_handlers, 0, 32);
    set_bytes(_devices, 0, 32);

    let slot: int8 = 0;
    while (slot < 32) {
        virtio_mmio_probe_device(slot);
        slot = slot + 1;
    }
}

@private
fn virtio_mmio_probe_device(slot: int8) -> int32 {
    let device = &_devices[slot];
    let virtio = virtio_mmio_reg(slot);

    let magic = virtio->magic;
    let version = virtio->version;
    let device_id = virtio->device_id;

    // parse magic
    if (magic != VIRTIO_MMIO_MAGIC)
        return VIRTIO_MMIO_BAD_MAGIC;

    // parse version
    if (version != VIRTIO_MMIO_VERSION_MODERN)
        return VIRTIO_MMIO_UNSUPPORTED_VERSION;

    // parse device ID
    if (device_id == VIRTIO_DEVICE_ID_INVALID)
        return VIRTIO_MMIO_INVALID_DEVICE;

    dprintk("[virtio_mmio@%lx] magic = 0x%x, version = %i, device_id = %i\n",
            virtio, magic, version, virtio->device_id);

    // acknowledge
    virtio->status = VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER;

    // get features
    virtio->device_features_sel = VIRTIO_FEATURES_LOW;
    let features_lo = virtio->device_features;

    virtio->device_features_sel = VIRTIO_FEATURES_HIGH;
    let features_hi = virtio->device_features;

    dprintk("[virtio_mmio@%lx] features: low=0x%x, high=0x%x\n",
            virtio, features_lo, features_hi);

    // negociate features
    dprintk("[virtio_mmio@%lx] negociating features\n", virtio);

    virtio->driver_features_sel = VIRTIO_FEATURES_LOW;
    virtio->driver_features = features_lo & (VIRTIO_BLK_F_BLK_SIZE | VIRTIO_BLK_F_SIZE_MAX);

    virtio->driver_features_sel = VIRTIO_FEATURES_HIGH;
    virtio->driver_features = features_hi & VIRTIO_F_VERSION_1; // must keep this

    virtio->status = virtio->status | VIRTIO_STATUS_FEATURES_OK;

    // check response
    if ((virtio->status & VIRTIO_STATUS_FEATURES_OK) == 0) {
        dprintk("[virtio_mmio@%lx] features rejected\n", virtio);
        virtio->status = virtio->status | VIRTIO_STATUS_FAILED;

        return VIRTIO_MMIO_NEGOTIATION_FAILED; // negotiation failed
    }

    // negotiation accepted
    dprintk("[virtio_mmio@%lx] features accepted\n", virtio);

    // set up virtqueue
    virtio->queue_sel = 0;

    let virtio_queue_num_max = virtio->queue_num_max;
    dprintk("[virtio_mmio@%lx] virtio_queue_num_max=%i\n", virtio, virtio_queue_num_max);

    if (virtio_queue_num_max < DEFAULT_VIRTIO_QUEUE_NUM) {
        dprintk("[virtio_mmio@%lx] device doesn't support queue_num=%i\n",
                virtio, DEFAULT_VIRTIO_QUEUE_NUM);
        return VIRTIO_MMIO_QUEUE_NUM_ERROR;
    }

    virtio->queue_num = DEFAULT_VIRTIO_QUEUE_NUM;

    let q = &device->queue;

    // Write physical addresses (split into low/high 32-bit halves)
    virtio->queue_desc_low = (&q->desc as uint64) as uint32;
    virtio->queue_desc_high = (&q->desc as uint64 >> 32) as uint32;

    virtio->queue_driver_low = (&q->avail as uint64) as uint32;
    virtio->queue_driver_high = (&q->avail as uint64 >> 32) as uint32;

    virtio->queue_device_low = (&q->used as uint64) as uint32;
    virtio->queue_device_high = (&q->used as uint64 >> 32) as uint32;

    // Mark the queue live
    virtio->queue_ready = 1;

    // initialize IRQ
    let virtio_mmio_irq: uint32;
    if (dtb_get_virtio_mmio_irq_number(slot as int32, &virtio_mmio_irq) != 0) {
        dprintk("[virtio_mmio] IRQ not found!!\n");
        return VIRTIO_MMIO_IRQ_NOT_FOUND;
    }

    _irq_to_slot[virtio_mmio_irq] = slot;

    dprintk("[virtio_mmio@%lx] Initializing IRQ: %i\n", virtio, virtio_mmio_irq);
    irq_register_handler(virtio_mmio_irq, virtio_mmio_irq_handler);
    gic_enable_irq(virtio_mmio_irq);

    // if everything goes well we add it to our table
    device->irq_id = virtio_mmio_irq;
    device->device_id = device_id;

    return 0;
}

/**
 * Scans initialized slots after `start` and returns the index of the first slot whose device_id
 * matches. Pass -1 to start from slot 0. Useful for iterating all devices of a given type when
 * multiple are present.
 *
 * @param device_id: device type to search for (see VIRTIO_DEVICE_ID_*)
 * @param start:     slot index to start after (exclusive); pass -1 to begin at 0
 *
 * @return slot index of the first matching device, or -1 if none found
 */
fn virtio_mmio_find_next_slot(device_id: uint32, start: int8) -> int8 {
    let idx: int8 = start + 1;
    while (idx < 32) {
        defer idx = idx + 1;
        if (_devices[idx].device_id == device_id)
            return idx;
    }

    return -1;
}

/**
 * Submits a synchronous read request to the virtio-blk device at the given slot. Builds a
 * 3-descriptor chain (request header → data buffer → status), posts it to the available ring, kicks
 * the device via QUEUE_NOTIFY, then spins via wfi() until the device writes the status byte.
 *
 * @param slot:          virtio MMIO slot index
 * @param sector_number: 512-byte sector to read from
 * @param data:          output buffer of at least VIRTIO_MMIO_BLK_SECTOR_SIZE bytes
 *
 * @return VIRTIO_BLK_S_OK (0) on success, VIRTIO_BLK_S_IOERR or VIRTIO_BLK_S_UNSUPP on device
 *         error, VIRTIO_MMIO_INVALID_DEVICE if the slot has no initialized device
 */
fn virtio_mmio_read(slot: int8, sector_number: uint64, data: uint8*) -> int32 {
    let device = &_devices[slot];

    if (device->device_id == VIRTIO_DEVICE_ID_INVALID) {
        dprintk("[virtio_mmio] Device slot %i is invalid!\n", slot);
        return VIRTIO_MMIO_INVALID_DEVICE;
    }

    // 1. Fill the header
    let hdr: struct virtio_blk_req_header;
    hdr.type = VIRTIO_BLK_T_IN;
    hdr.sector = sector_number;

    let status: uint8 = 0xFF; // device writes 0 on success, 1 on error, 2 = unsupported

    let q = &device->queue;

    // 2. Fill descriptor chain (indices 0, 1, 2)
    q->desc[0].addr = &hdr as uint64;
    q->desc[0].length = sizeof(struct virtio_blk_req_header) as uint32;
    q->desc[0].flags = VIRTQ_DESC_F_NEXT;
    q->desc[0].next = 1;

    q->desc[1].addr = data as uint64;
    q->desc[1].length = VIRTIO_MMIO_BLK_SECTOR_SIZE;
    q->desc[1].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
    q->desc[1].next = 2;

    q->desc[2].addr = &status as uint64;
    q->desc[2].length = 1;
    q->desc[2].flags = VIRTQ_DESC_F_WRITE;

    // 3. Post to available ring
    let idx: uint16 = q->avail.idx % DEFAULT_VIRTIO_QUEUE_NUM;
    q->avail.ring[idx] = 0; // head descriptor index
    q->avail.idx = q->avail.idx + 1;

    // 4. Kick the device
    let virtio = virtio_mmio_reg(slot);
    virtio->queue_notify = 0;

    // 5. Poll or wait for IRQ — device writes to used ring when done
    // In the IRQ handler, check used.idx advanced and read status byte
    while(status == VIRTIO_BLK_S_NONE)
        wfi();

    dprintk("[virtio_mmio@%lx] status = %u\n", virtio, status);

    return status as int32;
}

/**
 * IRQ handler for virtio MMIO device interrupts. Maps the IRQ ID to its slot via _irq_to_slot,
 * reads VIRTIO_INTERRUPT_STATUS to determine the cause (used-buffer or config change), acks it via
 * VIRTIO_INTERRUPT_ACK, then dispatches to the per-slot handler registered in _handlers[slot]. If
 * no handler is registered for the slot, returns ctx unchanged.
 *
 * @param irq: GIC IRQ ID, used to look up the virtio slot
 * @param ctx: saved register frame from the interrupted context
 *
 * @return ctx of the next task to run as returned by the slot handler,
 *         or ctx unchanged if no handler is registered
 */
fn virtio_mmio_irq_handler(irq: uint32, ctx: struct cpu_context*) -> struct cpu_context* {
    let slot = _irq_to_slot[irq];
    let fnc = _handlers[slot];

    let virtio = virtio_mmio_reg(slot);
    let status = virtio->interrupt_status;
    virtio->interrupt_ack = status;

    dprintk("[virtio_mmio@%lx] IRQ handler, status = %u\n", virtio, status);

    if (fnc == null) {
        dprintk("[virtio_mmio@%lx] Handler not found for slot %i!\n", virtio, slot);
        return ctx;
    }

    return fnc(slot, ctx);
}

@private @inline fn virtio_mmio_addr(slot: int8) -> uint64 {
    return VIRTIO_MMIO_BASE + (slot as uint64 * VIRTIO_MMIO_STRIDE);
}

@private @inline fn virtio_mmio_reg(slot: int8) -> struct virtio_mmio_regs* {
    return virtio_mmio_addr(slot) as struct virtio_mmio_regs*;
}