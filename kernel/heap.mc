import "debug";
import "cpu";
import "memory";

struct heap_block_header {
	size: uint64;
	used: bool;
	next: struct heap_block_header*;
}

struct heap {
	size: uint64;
	min_blk_size: uint16;
	head: struct heap_block_header*;
}

const HEADER_SIZE = sizeof(struct heap_block_header);

/**
 * Debug-only integrity check: walks the free/block list and halts if any block's
 * next pointer is not null, in-range, and 8-byte aligned -- i.e. catches a
 * header that has been stomped by an out-of-bounds write or use-after-free.
 * tag identifies the call site in the halt message.
 *
 * @param self: the heap to validate
 * @param tag:  short label naming the call site (printed on corruption)
 */
fn heap_check(self: struct heap*, tag: uint8*) {
	@if (DEBUG) {
		let lo = self->head as uint64;
		let hi = lo + self->size;

		let cur = self->head;
		until (cur == null) {
			let n = cur->next as uint64;
			if (n != 0 and (n < lo or n >= hi or (n & 7) != 0)) {
				printk("[heap] CORRUPT next=%p at block %p (%s)\n", cur->next, cur, tag);
				halt();
			}
			cur = cur->next;
		}
	}
}

/**
 * Initializes a heap over the region [ptr, ptr + size). Lays the whole region out as a single free
 * block whose header sits at ptr. Must be called before any heap_acquire/heap_release on this heap.
 *
 * @param self:         the heap to initialize
 * @param size:         total size of the backing region in bytes (header included)
 * @param min_blk_size: smallest payload worth splitting off; blocks are only split when the
 *                      remainder can hold a header plus at least this many bytes
 * @param ptr:          start of the backing region; the first block header is written here
 */
fn heap_init(self: struct heap*, size: uint64, min_blk_size: uint16, ptr: uint8*) {
	self->size = size;
	self->min_blk_size = min_blk_size;
	self->head = ptr as struct heap_block_header*;
	self->head->size = size - HEADER_SIZE;
	self->head->used = false;
	self->head->next = null;

	dprintk("[heap] heap initialized: addr=0x%p, size=%d B\n", ptr, self->head->size);
}

/**
 * Walks the block list and coalesces each run of adjacent free blocks into one, reclaiming the
 * headers between them. Called after heap_release to keep fragmentation down.
 *
 * @param self: the heap to compact
 */
@private
fn heap_merge_free_blocks(self: struct heap*) {
	let current = self->head;
	while (current != null and current->next != null) {
		if (!current->used and !current->next->used) {
			current->size = current->size + HEADER_SIZE + current->next->size;
			current->next = current->next->next;
		} else {
			current = current->next;
		}
	}
}

/**
 * Allocates size bytes from the heap using a first-fit scan of the block list. The request is
 * rounded up to 8 bytes, and the chosen block is split when the leftover can hold a header plus at
 * least min_blk_size bytes. The returned pointer is 8-byte aligned and points past the block header.
 *
 * @param self: the heap to allocate from
 * @param size: number of bytes requested
 *
 * @return pointer to the allocated payload, or null if size is 0 or no block is large enough
 */
fn heap_acquire(self: struct heap*, size: uint64) -> uint8* {
	heap_check(self, "acquire");

	if (size == 0) return null;

    // Align size to 8 bytes
	size = (size + 7) & (~7 as uint64);

	let current = self->head;
	until (current == null) {
		if (!current->used and current->size >= size) {
            let block_ptr = current as uint8*;

            // Split block if remainder is large enough to be useful
            if (current->size >= size + HEADER_SIZE + self->min_blk_size) {
                let new_block = &block_ptr[size + HEADER_SIZE] as struct heap_block_header*;

                new_block->size = current->size - size - HEADER_SIZE;
                new_block->used = false;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }

            current->used = true;
            let ptr = &block_ptr[HEADER_SIZE];

            if (ptr as uint64 >= self->head as uint64 + self->size) {
                printk("address=%p\n", ptr);
                halt();
            }

            return ptr;
		}

		current = current->next;
	}
	
    dprintk("[heap] kmalloc: out of memory!\n");

    let head = self->head;
    until (head == null) {
        printk("block at %p: size=%d B %s\n",
        	   head as uint64 + HEADER_SIZE, head->size,
        	   head->used ? "USED" : "FREE");

        head = head->next;
    }

    return null;
}

/**
 * Resizes a previously acquired block to new_size bytes. Keeps the block in place when it is already
 * big enough and not more than twice the request; otherwise acquires a new block, copies the smaller
 * of the old and new sizes, and releases the old one. A null ptr behaves like heap_acquire; a
 * new_size of 0 releases ptr and returns null.
 *
 * @param self:     the heap that owns ptr
 * @param ptr:      pointer returned by heap_acquire, or null
 * @param new_size: desired new payload size in bytes
 *
 * @return pointer to the (possibly relocated) payload, or null on failure / when new_size is 0 /
 *         when ptr lies outside this heap
 */
fn heap_resize(self: struct heap*, ptr: uint8*, new_size: uint64) -> uint8* {
	if (ptr == null) return heap_acquire(self, new_size);

	if (new_size == 0) {
		heap_release(self, ptr);
		return null;
	}
	
	let header = (ptr as uint64 - HEADER_SIZE) as struct heap_block_header*;

    if ((header as uint64) < (self->head as uint64) or
    	(header as uint64) >= (self->head as uint64 + self->size)) {
        printk("[heap] resize: pointer out of range!\n");
        return null;
    }

	if (header->size >= new_size and header->size <= new_size * 2)
		return ptr;

	let new_ptr = heap_acquire(self, new_size);
	if (new_ptr == null) {
		printk("[heap] resize: cannot acquire memory required\n");
		return null;
	}

	let copy_size = header->size < new_size ? header->size : new_size;
	bytecopy(new_ptr, ptr, copy_size);

	heap_release(self, ptr);

	return new_ptr;
}

/**
 * Releases a block previously returned by heap_acquire and coalesces it with any adjacent free
 * blocks. The block header is recovered from the bytes just before ptr.
 *
 * @param self: the heap that owns ptr
 * @param ptr:  pointer returned by heap_acquire
 *
 * @return  0 on success
 *         -1 if ptr is null
 *         -2 if ptr lies outside this heap's region
 *         -3 if the block was already free (double-free detected)
 */
fn heap_release(self: struct heap*, ptr: uint8*) -> int32 {
	heap_check(self, "release");

	if (ptr == null) return -1;

	let header = (ptr as uint64 - HEADER_SIZE) as struct heap_block_header*;

    // ptr should be inside heap
    if ((header as uint64) < (self->head as uint64) or
    	(header as uint64) >= (self->head as uint64 + self->size)) {
        printk("[heap] release: pointer out of range!\n");
        return -2;
    }

	// check if it's already been freed
    if (!header->used) {
        printk("[heap] release: double free detected!\n");
        return -3;
    }

    header->used = false;
    heap_merge_free_blocks(self);
    return 0;
}
