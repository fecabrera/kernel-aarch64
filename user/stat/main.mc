import "std";
import "filesystem/fs";
import "system/syscall";

@private
fn main(argc: int32, argv: uint8**) -> int32 {
    if (argc < 2) {
        println("usage: %s <path>", argv[0]);
        return -1;
    }

    let st: struct file_stat;
    let st_status = stat(argv[1], &st);
    if (st_status < 0) {
        println("stat() returned %lld!", st_status);
        return -2;
    }

    if ((st.st_mode & node_attrs::PERMISSIONS_MASK) > 0) {
        if ((st.st_mode & node_attrs::READ) > 0) print("r");
        if ((st.st_mode & node_attrs::WRITE) > 0) print("w");
        if ((st.st_mode & node_attrs::EXECUTE) > 0) print("x");
        print(" ");
    }
    println("%llu", st.st_size);
    
    return 0;
}
