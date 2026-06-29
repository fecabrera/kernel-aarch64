import "system/syscall";

@extern fn main(argc: int64, argv: char*) -> int64;

fn entry(argc: int64, argv: char*) {
    exit(main(argc, argv));
}