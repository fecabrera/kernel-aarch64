import "std";
import "fdt";
import "endian";
import "system/fs";
import "system/syscall";
import "libc/string";

fn main(argc: int64, argv: char**) -> int64 {
    if (argc < 2) {
        println("usage: %s <file> [path]", argv[0]);
        return 1;
    }

    let fd = open(argv[1], open_mode::READ);
    if (fd < 0) {
        println("could not open \"%s\", open() returned %lld", argv[1], fd);
        return 1;
    }

    defer close(fd);
    
    let status: int64;
    let st: file_stat;

    status = fstat(fd, &st);
    if (status < 0) {
        println("could not read \"%s\", fstat() returned %lld", argv[1], status);
        return 1;
    }

    let buf = alloc<byte>(st.st_size);
    defer dealloc(buf);

    status = read(fd, buf, st.st_size);
    if (status < 0) {
        println("could not read \"%s\", read() returned %lld", argv[1], status);
        return 1;
    }

    let obj: fdt_file;
    fdt_file_init(&obj, buf);
    defer fdt_file_destroy(&obj);
    
    fdt_file_build_tree(&obj);

    if (obj.tree == null) {
        println("fdt_file.tree returned null");
        return 1;
    }
    

    if (obj.tree->dt_node == null) {
        println("fdt_file.tree->dt_node is null");
        return 1;
    }

    let r = struct range { start = 2, end = argc };
    for i in &r {
        let node = fdt_file_find_node(&obj, argv[i]);
        if (node == null) continue;
        fdt_dump_tree_node(node, obj.dt_strings, 0);
    }

    return 0;
}