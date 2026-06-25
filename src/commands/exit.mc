import "std";
import "libc/stdlib";

fn command_exit(argc: int64, argv: uint8**) -> int64 {
    let status: int64 = argc > 1 ? atoll(argv[1]) : 0;
    println("exiting with status %lld", status);
    return status;
}
