import "std";
import "system/syscall";
import "libc/stdlib";

@private
fn main(argc: int64, argv: char**) -> int64 {
    if (argc < 2) {
        println("usage: %s <seconds>", argv[0]);
        return -1;
    }

    let ret: int64 = msleep(atoll(argv[1]) as uint64);
    if (ret < 0) {
        println("failed to sleep");
        return ret;
    }

    return 0;
}
