import "system/syscall";

@extern fn main(argc: int64, argv: uint8*) -> int64;

fn entry(argc: int64, argv: uint8*) {
    exit(main(argc, argv));
}