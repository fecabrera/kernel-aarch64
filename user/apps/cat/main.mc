import "std";
import "memory";
import "system/syscall";
import "system/fs";

fn main(argc: int64, argv: char**) -> int64 {
    if (argc < 2) {
        println("usage: %s <path>", argv[0]);
        return -1;
    }

    let fd = open(argv[1], open_mode::READ);
    if (fd < 0) {
        println("open() returned %lld!", fd);
        return -2;
    }
    
    defer close(fd);

    let st: struct file_stat;
    let st_status = fstat(fd, &st);
    if (st_status < 0) {
        println("fstat() returned %lld!", st_status);
        return -3;
    }
    
    let buffer: uint8* = alloc<uint8>(st.st_size + 1);
    defer dealloc(buffer);

    let r_status = read(fd, buffer, st.st_size);
    if (r_status < 0) {
        println("read() returned %lld!", r_status);
        return -4;
    }

    buffer[st.st_size] = '\0';
    println("%s", buffer);

    return 0;
}
