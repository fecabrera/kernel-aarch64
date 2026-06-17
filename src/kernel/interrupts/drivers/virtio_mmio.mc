import "cpu";

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

/**
 * Scans all 32 virtio MMIO slots, initializes each valid device found (modern version, non-zero
 * device ID), reads its feature words, and registers its IRQ handler with the GIC. Must be called
 * after gic_init and irq_init, and before irq_enable.
 */
@extern fn virtio_mmio_init();

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
@extern fn virtio_mmio_find_next_slot(device_id: uint32, start: int8) -> int8;

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
@extern fn virtio_mmio_read(slot: int8, sector_number: uint64, data: uint8*) -> int32;

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
@extern fn virtio_mmio_irq_handler(irq: int32, ctx: struct cpu_context*) -> struct cpu_context*;