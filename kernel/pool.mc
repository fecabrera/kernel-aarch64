import "debug";

struct pool {
    pool_base: uint64;
    pool_size: uint64;
    blk_size: uint64;
    first_free_entry: struct pool_entry*;
    last_free_entry: struct pool_entry*;
}

struct pool_entry {
    count: uint64;
    prev: struct pool_entry*;
    next: struct pool_entry*;
}

const POOL_INIT_ERROR_UNALIGNED = -1;

fn pool_init(self: struct pool*, pool_base: uint64, pool_size: uint64,
             blk_size: uint64) -> int32 {
    let blk_cnt = pool_size / blk_size;

    if ((pool_size % blk_size) != 0) {
        dprintk("[pool] init: pool size must be a multiple of blk size!\n");
        return POOL_INIT_ERROR_UNALIGNED;
    }

    // write header
    let header = pool_base as struct pool_entry*;
    header->count = blk_cnt;
    header->prev = null;
    header->next = null;

    // initialize pool
    self->pool_base = pool_base;
    self->pool_size = pool_size;
    self->blk_size = blk_size;
    self->first_free_entry = header;
    self->last_free_entry = header;

    dprintk("[pool] init: addr=0x%p, blk_cnt=%llu, size=%llu B, blk_size=%llu B\n",
           pool_base, blk_cnt, pool_size, blk_size);

    return 0;
}

fn pool_dump(self: struct pool*) {
@if (DEBUG) {
    dprintk("[pool] dump: first=0x%p, last=0x%p\n", self->first_free_entry, self->last_free_entry);
    dprintk("[pool] free block:\n");

    let current = self->first_free_entry;
    until (current == null) {
        dprintk("[pool]   addr=0x%p, count=%llu\n", current, current->count);
        current = current->next;
    }
}
}

fn pool_destroy(self: struct pool*) {
    if (self == null) {
        dprintk("[pool] destroy: self is null\n");
        return;
    }

    self->pool_base = 0;
    self->pool_size = 0;
    self->blk_size = 0;
    self->first_free_entry = null;
    self->last_free_entry = null;
}

fn pool_alloc(self: struct pool*, count: uint64) -> byte* {
    let current = self->first_free_entry;
    until (current == null) {
        if (current->count >= count) {
            dprintk("[pool] alloc: found free entry at 0x%p\n", current);

            // split entry if necessary
            if (current->count > count) {
                dprintk("[pool] alloc: splitting entry\n");

                // fill next
                let next = (current as uint64 + (count * self->blk_size)) as struct pool_entry*;
                next->count = current->count - count;
                next->next = current->next;

                dprintk("[pool] alloc: next=0x%p\n", next);

                // update linked list
                if (current->next != null)
                    current->next->prev = next;

                // insert after current
                current->next = next;

                // set to last free entry if selected
                if (current == self->last_free_entry) {
                    dprintk("[pool] alloc: setting next as last free entry\n");
                    self->last_free_entry = next;
                }
            }

            // update linked list
            if (current->prev != null)
                current->prev->next = current->next;

            if (current->next != null)
                current->next->prev = current->prev;

            // update first free entry if selected
            if (current == self->first_free_entry) {
                dprintk("[pool] alloc: updating first free entry\n");
                self->first_free_entry = current->next;
            }

            // update last free entry if selected
            if (current == self->last_free_entry) {
                dprintk("[pool] alloc: updating last free entry\n");
                self->last_free_entry = current->prev;
            }

            dprintk("[pool] alloc: returning entry at 0x%p\n", current);
            pool_dump(self);
            
            // return ptr;
            return current as byte*;
        }

        current = current->next;
    }

    dprintk("[pool] alloc: out of memory!\n");

    pool_dump(self);
    
    return null;
}

/**
 * Coalesces two adjacent free entries. If `left`'s extent ends exactly where
 * `right`'s begins, absorbs `right` into `left` (extends left->count, unlinks
 * right, and fixes the trailing neighbor and cached last_free_entry). No-op if
 * either is null or the two are not physically adjacent. `right` is never the
 * first free entry (it is the higher-address side), so first_free_entry needs
 * no fixup.
 *
 * @param self:  the pool
 * @param left:  lower-address entry; survives the merge
 * @param right: higher-address entry; absorbed and unlinked
 *
 * @return true if the entries were merged, false otherwise
 */
@private
fn pool_try_merge(self: struct pool*, left: struct pool_entry*, right: struct pool_entry*) -> bool {
    if (left == null or right == null)
        return false;

    // adjacent only if left's extent ends exactly at right's start
    if (left as uint64 + (left->count * self->blk_size) != right as uint64)
        return false;

    left->count = left->count + right->count;
    left->next = right->next;

    if (right->next != null)
        right->next->prev = left;

    if (right == self->last_free_entry)
        self->last_free_entry = left;

    return true;
}

/**
 * Frees a `count`-block allocation starting at ptr. Splices the entry into the
 * address-ordered free list and coalesces it with its right and left neighbors
 * via pool_try_merge.
 *
 * @param self:  the pool
 * @param ptr:   pointer previously returned by pool_alloc
 * @param count: number of blocks the allocation spans
 *
 * @return 0 on success, -1 if ptr is null
 */
fn pool_free(self: struct pool*, ptr: byte*, count: uint64) -> int32 {
    if (ptr == null) {
        dprintk("[pool] free: null pointer!\n");
        return -1;
    }

    let entry = ptr as struct pool_entry*;

    dprintk("[pool] free: addr=0x%p, count=%llu\n", ptr, count);

    entry->count = count;

    // find the first free entry after `entry` in address order; `entry` is
    // spliced in just before it (null => append at the tail)
    let next = self->first_free_entry;
    until (next == null or (next as uint64) > (entry as uint64))
        next = next->next;

    let prev = next != null ? next->prev : self->last_free_entry;

    // splice entry between prev and next, updating the cached endpoints
    entry->prev = prev;
    entry->next = next;

    if (prev != null)
        prev->next = entry;
    else
        self->first_free_entry = entry;

    if (next != null)
        next->prev = entry;
    else
        self->last_free_entry = entry;

    // fold the right neighbor into entry, then fold the result into the left one
    pool_try_merge(self, entry, entry->next);
    pool_try_merge(self, entry->prev, entry);

    pool_dump(self);

    return 0;
}
