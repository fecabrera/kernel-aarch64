import "std";
import "memory";
import "system/syscall";
import "elf/elf64";

fn command_elf(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2) {
        println("usage: %s <path>", argv[0]);
        return -1;
    }

    // open file
    let fd = open(argv[1], FS_FILE_ATTRS_READ);
    if (fd < 0) {
        println("open() returned %lld!", fd);
        return -2;
    }

    defer close(fd);

    // get file size
    let st: struct file_stat;
    let st_status = fstat(fd, &st);
    if (st_status < 0) {
        println("fstat() returned %lld!", st_status);
        return -3;
    }

    // allocate buffer
    let buf: uint8* = alloc<uint8>(st.st_size);
    defer dealloc(buf);

    // read file
    let rd_status = read(fd, buf, st.st_size);
    if (rd_status < 0) {
        println("read() returned %lld!", rd_status);
        return -4;
    }

    // check if it's a valid elf header
    if (!is_elf(buf)) {
        println("\"%s\" is not an ELF file!", argv[1]);
        return -5;
    }

    // parse elf file
    let elf_file: struct elf64_file;
    let elf_rd_status = elf_read(buf, st.st_size, &elf_file);
    if (elf_rd_status < 0) {
        println("elf_read() returned %u", elf_rd_status);
        return -6;
    }

    // allocate memory
    let exec_buf: uint8* = alloc<uint8>(elf_file.memsz);
    defer dealloc(exec_buf);
    
    // resolve symbols, relocate if necessary, and load sectors to memory
    elf64_load(&elf_file, exec_buf as uint64);
    
    // get entry symbol
    let entry = elf64_locate_symbol(&elf_file, "main") as fn () -> int32;

    // execute
    let ret = entry();

    // cleanup
    elf64_unload(&elf_file);

    return ret as int64;
}
