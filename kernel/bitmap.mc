// Generic bit-array helpers over a backing array of unsigned words of type T.
// Bit i lives in word i / (8 * sizeof(T)) at offset i % (8 * sizeof(T)). The
// caller owns the backing storage and is responsible for its size.

/**
 * Sets bit i (marks it 1).
 *
 * @param bitmap: backing word array
 * @param i:      bit index
 */
@inline
fn bit_set<T>(bitmap: T*, i: uint64) {
    let bpw = 8 * sizeof(T);
    bitmap[i / bpw] = bitmap[i / bpw] | (1 as T << (i % bpw));
}

/**
 * Clears bit i (marks it 0).
 *
 * @param bitmap: backing word array
 * @param i:      bit index
 */
@inline
fn bit_clear<T>(bitmap: T*, i: uint64) {
    let bpw = 8 * sizeof(T);
    bitmap[i / bpw] = bitmap[i / bpw] & ~(1 as T << (i % bpw));
}

/**
 * Tests bit i.
 *
 * @param bitmap: backing word array
 * @param i:      bit index
 *
 * @return true if bit i is set, false otherwise
 */
@inline
fn bit_test<T>(bitmap: T*, i: uint64) -> bool {
    let bpw = 8 * sizeof(T);
    return (bitmap[i / bpw] & (1 as T << (i % bpw))) != 0;
}
