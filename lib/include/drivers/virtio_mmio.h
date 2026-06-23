#pragma once

#include <arch/irq.h>
#include <stdint.h>

#define DEFAULT_VIRTIO_QUEUE_NUM 64

// ── Magic / version ──────────────────────────────────────────────────────────

#define VIRTIO_MMIO_MAGIC 0x74726976
#define VIRTIO_MMIO_VERSION_LEGACY 1
#define VIRTIO_MMIO_VERSION_MODERN 2

// ── Device IDs ───────────────────────────────────────────────────────────────

#define VIRTIO_DEVICE_ID_INVALID 0
#define VIRTIO_DEVICE_ID_NET 1
#define VIRTIO_DEVICE_ID_BLOCK 2
#define VIRTIO_DEVICE_ID_CONSOLE 3
#define VIRTIO_DEVICE_ID_RNG 4
#define VIRTIO_DEVICE_ID_GPU 16
#define VIRTIO_DEVICE_ID_INPUT 18

// ── MMIO base and per-device stride ──────────────────────────────────────────

#define VIRTIO_MMIO_BASE 0x0a000000UL
#define VIRTIO_MMIO_STRIDE 0x200
#define VIRTIO_MMIO_ADDR(n) (VIRTIO_MMIO_BASE + (n) * VIRTIO_MMIO_STRIDE)

// ── Register accessors ───────────────────────────────────────────────────────

/**
 * virtio-mmio (modern, v2) register block for one device. All registers are
 * 32-bit; reserved gaps are skipped to keep the named registers at their
 * architectural offsets. Each of the 32 slots has its own block at
 * VIRTIO_MMIO_ADDR(n); access it through the VIRTIO(n) pointer (e.g.
 * VIRTIO(slot)->magic).
 */
struct virtio_mmio_regs {
    volatile uint32_t magic;                  // 0x000 R   magic value
    volatile uint32_t version;                // 0x004 R   device version
    volatile uint32_t device_id;              // 0x008 R   device type
    volatile uint32_t vendor_id;              // 0x00c R   vendor
    volatile uint32_t device_features;        // 0x010 R   features offered
    volatile uint32_t device_features_sel;    // 0x014 W   select feature word
    uint32_t _reserved0[(0x020 - 0x018) / 4]; // 0x018–0x01c
    volatile uint32_t driver_features;        // 0x020 W   features accepted
    volatile uint32_t driver_features_sel;    // 0x024 W   select feature word
    uint32_t _reserved1[(0x030 - 0x028) / 4]; // 0x028–0x02c
    volatile uint32_t queue_sel;              // 0x030 W   select virtqueue
    volatile uint32_t queue_num_max;          // 0x034 R   max queue size
    volatile uint32_t queue_num;              // 0x038 W   set queue size
    uint32_t _reserved2[(0x044 - 0x03c) / 4]; // 0x03c–0x040
    volatile uint32_t queue_ready;            // 0x044 RW  activate queue
    uint32_t _reserved3[(0x050 - 0x048) / 4]; // 0x048–0x04c
    volatile uint32_t queue_notify;           // 0x050 W   kick device
    uint32_t _reserved4[(0x060 - 0x054) / 4]; // 0x054–0x05c
    volatile uint32_t interrupt_status;       // 0x060 R   pending interrupts
    volatile uint32_t interrupt_ack;          // 0x064 W   clear interrupts
    uint32_t _reserved5[(0x070 - 0x068) / 4]; // 0x068–0x06c
    volatile uint32_t status;                 // 0x070 RW  device status
    uint32_t _reserved6[(0x080 - 0x074) / 4]; // 0x074–0x07c
    volatile uint32_t queue_desc_low;         // 0x080 W   descriptor table PA low
    volatile uint32_t queue_desc_high;        // 0x084 W   descriptor table PA high
    uint32_t _reserved7[(0x090 - 0x088) / 4]; // 0x088–0x08c
    volatile uint32_t queue_driver_low;       // 0x090 W   available ring PA low
    volatile uint32_t queue_driver_high;      // 0x094 W   available ring PA high
    uint32_t _reserved8[(0x0a0 - 0x098) / 4]; // 0x098–0x09c
    volatile uint32_t queue_device_low;       // 0x0a0 W   used ring PA low
    volatile uint32_t queue_device_high;      // 0x0a4 W   used ring PA high
    uint32_t _reserved9[(0x0fc - 0x0a8) / 4]; // 0x0a8–0x0f8
    volatile uint32_t config_gen;             // 0x0fc R   config generation counter
    volatile uint32_t config[];               // 0x100 RW  device-specific config
};

#define VIRTIO(n) ((struct virtio_mmio_regs *)VIRTIO_MMIO_ADDR(n))

// ── Status register bits (write in order during init) ────────────────────────

#define VIRTIO_STATUS_ACKNOWLEDGE (1 << 0) // driver noticed the device
#define VIRTIO_STATUS_DRIVER (1 << 1)      // driver knows how to drive it
#define VIRTIO_STATUS_DRIVER_OK (1 << 2)   // driver is ready
#define VIRTIO_STATUS_FEATURES_OK (1 << 3) // feature negotiation complete
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET (1 << 6)
#define VIRTIO_STATUS_FAILED (1 << 7) // driver gave up

// ── Interrupt status / ack bits ──────────────────────────────────────────────

#define VIRTIO_INT_USED_BUFFER (1 << 0)   // device consumed a buffer
#define VIRTIO_INT_CONFIG_CHANGE (1 << 1) // device config changed

// ── Feature word selectors ───────────────────────────────────────────────────

#define VIRTIO_FEATURES_LOW 0  // bits 0–31
#define VIRTIO_FEATURES_HIGH 1 // bits 32–63

// ── Transport feature bits (word 1, i.e. bit index relative to 32) ───────────

#define VIRTIO_F_NOTIFY_ON_EMPTY (1 << 0)    // bit 32 global
#define VIRTIO_F_ANY_LAYOUT (1 << 2)         // bit 34 global
#define VIRTIO_F_RING_INDIRECT_DESC (1 << 5) // bit 37 global
#define VIRTIO_F_RING_EVENT_IDX (1 << 6)     // bit 38 global
#define VIRTIO_F_VERSION_1 (1 << 29)         // bit 61 global — must negotiate for modern

// ── Block device feature bits (word 0) ───────────────────────────────────────

#define VIRTIO_BLK_F_SIZE_MAX (1 << 1)
#define VIRTIO_BLK_F_SEG_MAX (1 << 2)
#define VIRTIO_BLK_F_GEOMETRY (1 << 4)
#define VIRTIO_BLK_F_RO (1 << 5)
#define VIRTIO_BLK_F_BLK_SIZE (1 << 6)
#define VIRTIO_BLK_F_FLUSH (1 << 9)
#define VIRTIO_BLK_F_TOPOLOGY (1 << 10)
#define VIRTIO_BLK_F_CONFIG_WCE (1 << 11)
#define VIRTIO_BLK_F_DISCARD (1 << 13)
#define VIRTIO_BLK_F_WRITE_ZEROES (1 << 14)

//
#define VIRTIO_MMIO_BLK_SECTOR_SIZE 512

// Block request types — written into virtio_blk_req_header.type
#define VIRTIO_BLK_T_IN 0  // read from device into driver buffer
#define VIRTIO_BLK_T_OUT 1 // write from driver buffer to device

// Block request statuses — written by the device into the status byte of the request
#define VIRTIO_BLK_S_OK 0      // success
#define VIRTIO_BLK_S_IOERR 1   // device I/O error
#define VIRTIO_BLK_S_UNSUPP 2  // unsupported request type
#define VIRTIO_BLK_S_NONE 0xFF // sentinel: device has not yet responded

// ── Virtqueue descriptor flags ───────────────────────────────────────────────

#define VIRTQ_DESC_F_NEXT (1 << 0)     // descriptor is part of a chain; .next is valid
#define VIRTQ_DESC_F_WRITE (1 << 1)    // device writes to this buffer (driver-read-only if clear)
#define VIRTQ_DESC_F_INDIRECT (1 << 2) // buffer contains a table of descriptors

// Probing errors

#define VIRTIO_MMIO_BAD_MAGIC -1
#define VIRTIO_MMIO_UNSUPPORTED_VERSION -2
#define VIRTIO_MMIO_INVALID_DEVICE -3
#define VIRTIO_MMIO_NEGOTIATION_FAILED -4
#define VIRTIO_MMIO_QUEUE_NUM_ERROR -5
#define VIRTIO_MMIO_IRQ_NOT_FOUND -6

typedef int8_t virtio_slot_t;

struct virtq_desc {
    uint64_t addr;  // physical address of the buffer
    uint32_t len;   // length in bytes
    uint16_t flags; // VIRTQ_DESC_F_NEXT, VIRTQ_DESC_F_WRITE, etc.
    uint16_t next;  // index of next descriptor in chain (if F_NEXT set)
};

struct virtq_avail {
    uint16_t flags;                          // VIRTQ_AVAIL_F_NO_INTERRUPT
    uint16_t idx;                            // next slot the driver will write
    uint16_t ring[DEFAULT_VIRTIO_QUEUE_NUM]; // descriptor indices
};

struct virtq_used {
    uint16_t flags; // VIRTQ_USED_F_NO_NOTIFY
    uint16_t idx;   // next slot the device will write
    struct {
        uint32_t id;  // descriptor chain head index
        uint32_t len; // bytes written (for reads)
    } ring[DEFAULT_VIRTIO_QUEUE_NUM];
};

struct virtq {
    struct virtq_desc desc[DEFAULT_VIRTIO_QUEUE_NUM];
    struct virtq_avail avail;
    struct virtq_used used;
};

struct virtio_blk_req_header {
    uint32_t type; // VIRTIO_BLK_T_IN (0) = read, VIRTIO_BLK_T_OUT (1) = write
    uint32_t reserved;
    uint64_t sector; // 512-byte sector number to start from
};

struct virtio_mmio_device {
    uint32_t irq_id;
    uint32_t device_id;
    struct virtq queue;
};

typedef struct cpu_context *(*virtio_mmio_handler_t)(int i, struct cpu_context *);

/**
 * Scans all 32 virtio MMIO slots, initializes each valid device found (modern version, non-zero
 * device ID), reads its feature words, and registers its IRQ handler with the GIC. Must be called
 * after gic_init and irq_init, and before irq_enable.
 */
void virtio_mmio_init();

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
virtio_slot_t virtio_mmio_find_next_slot(uint32_t device_id, virtio_slot_t start);

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
int virtio_mmio_read(virtio_slot_t slot, uint64_t sector_number, uint8_t *data);

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
struct cpu_context *virtio_mmio_irq_handler(int irq, struct cpu_context *ctx);
