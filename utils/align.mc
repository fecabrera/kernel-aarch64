@inline
fn aligned<T>(x: T, align: uint64) -> T {
    return ((x + align - 1) & ~(align - 1)) as T;
}