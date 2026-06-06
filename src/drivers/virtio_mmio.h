#pragma once

#include <stdint.h>
#include <arch/irq.h>

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

#define VIRTIO_REG(n, off) (*(volatile uint32_t *)(VIRTIO_MMIO_ADDR(n) + (off)))

#define VIRTIO_MAGIC(n) VIRTIO_REG(n, 0x000)               // R   magic value
#define VIRTIO_VERSION(n) VIRTIO_REG(n, 0x004)             // R   device version
#define VIRTIO_DEVICE_ID(n) VIRTIO_REG(n, 0x008)           // R   device type
#define VIRTIO_VENDOR_ID(n) VIRTIO_REG(n, 0x00c)           // R   vendor
#define VIRTIO_DEVICE_FEATURES(n) VIRTIO_REG(n, 0x010)     // R   features offered
#define VIRTIO_DEVICE_FEATURES_SEL(n) VIRTIO_REG(n, 0x014) // W   select feature word
#define VIRTIO_DRIVER_FEATURES(n) VIRTIO_REG(n, 0x020)     // W   features accepted
#define VIRTIO_DRIVER_FEATURES_SEL(n) VIRTIO_REG(n, 0x024) // W   select feature word
#define VIRTIO_QUEUE_SEL(n) VIRTIO_REG(n, 0x030)           // W   select virtqueue
#define VIRTIO_QUEUE_NUM_MAX(n) VIRTIO_REG(n, 0x034)       // R   max queue size
#define VIRTIO_QUEUE_NUM(n) VIRTIO_REG(n, 0x038)           // W   set queue size
#define VIRTIO_QUEUE_READY(n) VIRTIO_REG(n, 0x044)         // RW  activate queue
#define VIRTIO_QUEUE_NOTIFY(n) VIRTIO_REG(n, 0x050)        // W   kick device
#define VIRTIO_INTERRUPT_STATUS(n) VIRTIO_REG(n, 0x060)    // R   pending interrupts
#define VIRTIO_INTERRUPT_ACK(n) VIRTIO_REG(n, 0x064)       // W   clear interrupts
#define VIRTIO_STATUS(n) VIRTIO_REG(n, 0x070)              // RW  device status
#define VIRTIO_QUEUE_DESC_LOW(n) VIRTIO_REG(n, 0x080)      // W   descriptor table PA low
#define VIRTIO_QUEUE_DESC_HIGH(n) VIRTIO_REG(n, 0x084)     // W   descriptor table PA high
#define VIRTIO_QUEUE_DRIVER_LOW(n) VIRTIO_REG(n, 0x090)    // W   available ring PA low
#define VIRTIO_QUEUE_DRIVER_HIGH(n) VIRTIO_REG(n, 0x094)   // W   available ring PA high
#define VIRTIO_QUEUE_DEVICE_LOW(n) VIRTIO_REG(n, 0x0a0)    // W   used ring PA low
#define VIRTIO_QUEUE_DEVICE_HIGH(n) VIRTIO_REG(n, 0x0a4)   // W   used ring PA high
#define VIRTIO_CONFIG_GEN(n) VIRTIO_REG(n, 0x0fc)          // R   config generation counter
#define VIRTIO_CONFIG(n) VIRTIO_REG(n, 0x100)              // RW  device-specific config

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

struct virtq_desc
{
    uint64_t addr;  // physical address of the buffer
    uint32_t len;   // length in bytes
    uint16_t flags; // VIRTQ_DESC_F_NEXT, VIRTQ_DESC_F_WRITE, etc.
    uint16_t next;  // index of next descriptor in chain (if F_NEXT set)
};

struct virtq_avail
{
    uint16_t flags;                          // VIRTQ_AVAIL_F_NO_INTERRUPT
    uint16_t idx;                            // next slot the driver will write
    uint16_t ring[DEFAULT_VIRTIO_QUEUE_NUM]; // descriptor indices
};

struct virtq_used
{
    uint16_t flags; // VIRTQ_USED_F_NO_NOTIFY
    uint16_t idx;   // next slot the device will write
    struct
    {
        uint32_t id;  // descriptor chain head index
        uint32_t len; // bytes written (for reads)
    } ring[DEFAULT_VIRTIO_QUEUE_NUM];
};

struct virtq
{
    struct virtq_desc desc[DEFAULT_VIRTIO_QUEUE_NUM];
    struct virtq_avail avail;
    struct virtq_used used;
};

struct virtio_blk_req_header
{
    uint32_t type; // VIRTIO_BLK_T_IN (0) = read, VIRTIO_BLK_T_OUT (1) = write
    uint32_t reserved;
    uint64_t sector; // 512-byte sector number to start from
};

struct virtio_mmio_device
{
    uint32_t irq_id;
    uint32_t device_id;
    struct virtq queue;
};

typedef struct cpu_context *(*virtio_mmio_handler_t)(int i, struct cpu_context *);

/**
 * Scans all 32 virtio MMIO slots, initializes each valid device found
 * (modern version, non-zero device ID), reads its feature words, and
 * registers its IRQ handler with the GIC. Must be called after gic_init
 * and irq_init, and before irq_enable.
 */
void virtio_mmio_init();

/**
 * Scans initialized slots after `start` and returns the index of the first
 * slot whose device_id matches. Pass -1 to start from slot 0. Useful for
 * iterating all devices of a given type when multiple are present.
 *
 * @param device_id: device type to search for (see VIRTIO_DEVICE_ID_*)
 * @param start:     slot index to start after (exclusive); pass -1 to begin at 0
 *
 * @return slot index of the first matching device, or -1 if none found
 */
int virtio_mmio_find_next_slot(uint32_t device_id, int start);

/**
 * Submits a synchronous read request to the virtio-blk device at the given
 * slot. Builds a 3-descriptor chain (request header → data buffer → status),
 * posts it to the available ring, kicks the device via QUEUE_NOTIFY, then
 * sleeps via syscall_msleep until the device writes the status byte.
 *
 * @param slot:          virtio MMIO slot index (e.g. VIRTIO_MMIO_SLOT_BLK)
 * @param sector_number: 512-byte sector to read from
 * @param data:          output buffer of at least VIRTIO_MMIO_BLK_SECTOR_SIZE bytes
 *
 * @return VIRTIO_BLK_S_OK (0) on success, VIRTIO_BLK_S_IOERR or
 *         VIRTIO_BLK_S_UNSUPP on device error, VIRTIO_MMIO_INVALID_DEVICE
 *         if the slot has no initialized device.
 */
int virtio_mmio_read(int slot, uint64_t sector_number, uint8_t *data);

/**
 * IRQ handler for virtio MMIO device interrupts. Maps the IRQ ID to its
 * slot via _irq_to_slot, reads VIRTIO_INTERRUPT_STATUS to determine the
 * cause (used-buffer or config change), acks it via VIRTIO_INTERRUPT_ACK,
 * then dispatches to the per-slot handler registered in _handlers[slot].
 * If no handler is registered for the slot, returns ctx unchanged.
 *
 * @param irq: GIC IRQ ID, used to look up the virtio slot
 * @param ctx: saved register frame from the interrupted context
 *
 * @return ctx of the next task to run as returned by the slot handler,
 *         or ctx unchanged if no handler is registered
 */
struct cpu_context *virtio_mmio_irq_handler(int irq, struct cpu_context *ctx);

/**
 * Reads the FAT32 filesystem on the virtio-blk device at the given slot.
 * Validates the boot sector signature, parses the BPB, reads the full FAT
 * table sector-by-sector, builds a cluster chain queue, then recursively
 * traverses all directories via fat32_read_cluster, populating an fs_node
 * tree rooted at the volume label. Mounts the root node at "/volumes" via
 * fs_mount as a side effect.
 *
 * @param slot: virtio MMIO slot index of the block device to read
 *
 * @return root fs_node of the parsed filesystem tree, or NULL on any I/O
 *         error, invalid FAT32 signature, or allocation failure.
 */
struct fs_node *virtio_mmio_initialize_fat32_device(int slot);
