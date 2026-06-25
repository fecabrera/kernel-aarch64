import "debug";
import "std";
import "list";
import "string";
import "memory";
import "scheduler";
import "elf/elf64";
import "filesystem/file";
import "filesystem/fs";
import "filesystem/vfs";
import "filesystem/drivers/fat32";
import "libc/ctype";
import "libc/stdlib";
import "system/syscall";
import "commands/cat";

@static
let available_commands: uint8*[][2] = [
    ["ls [path]", "list a folder's entries"],
    ["cd <path>", "change the working directory"],
    ["cat <path>", "print a file"],
    ["mount <device> [mountpoint]", "mount a FAT32 block device"],
    ["help", "show this message"],
];

@static
let dirs: uint8*[] = ["bin"];

@private
fn command_help(argc: int64, argv: uint8**) -> int64 {
    println("available commands:");

    let i: uint64 = 0;
    while (i < len(available_commands)) {
        defer i = i + 1;
        let command = available_commands[i];
        println("    %-28s %s", command[0], command[1]);
    }
    
    return 0;
}

@private
fn command_mount(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2) {
        println("usage: %s <device> [mountpoint]", argv[0]);
        return -1;
    }

    let mountpoint: uint8* = argc > 2 ? argv[2] : null;

    let status = fat32_mount(argv[1], mountpoint);
    if (status < 0) {
        println("fat32_mount() returned %d!", status);
        return -2;
    }

    println("block device \"%s\" mounted!", argv[1]);
    return 0;
}

@private
fn console_ls(argc: int64, argv: uint8**) -> int64 {
    let filename: uint8* = argc > 1 ? argv[1] : "/";
    
    let proc = scheduler_get_current_process();
    
    let node = vfs_get_node_for_path(filename, proc->cwd);
    if (node == null) {
        println("\"%s\" not found!", filename);
        return -1;
    }

    if ((node->attrs & node_attrs::TYPE_MASK) != node_attrs::DIR) {
        println("\"%s\" is not a folder!", filename);
        return -2;
    }

    until ((node->attrs & node_attrs::LINK) == 0)
        node = node->child;

    let current = node->child;
    until (current == null) {
        defer current = current->next;
        if ((current->attrs & node_attrs::HIDDEN) != 0)
            continue;
        
        println("%s", current->name);
    }

    return 0;
}

/**
 * Changes the calling process's current working directory to the named child
 * of its cwd. Unlike the other built-ins, cd is dispatched directly (not via
 * console_run_command) so the cwd update lands in the console process itself
 * rather than a short-lived forked child. The target must be an immediate child
 * folder of the current directory; LINK nodes (e.g. "." / "..") are followed to
 * their target.
 *
 * @param argc: number of arguments
 * @param argv: argv[0] is the command name; argv[1] is the destination folder
 *
 * @return 0 on success, -1 if no path was given, -2 if the path is not found or
 *         is not a folder
 */
@private
fn console_cd(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2) {
        println("usage: %s <path>", argv[0]);
        return -1;
    }
    
    let proc = scheduler_get_current_process();
    let node = vfs_get_node_for_path(argv[1], proc->cwd);
    if (node == null) {
        println("\"%s\" not found!", argv[1]);
        return -2;
    }

    if ((node->attrs & node_attrs::TYPE_MASK) != node_attrs::DIR) {
        println("\"%s\" is not a folder!", argv[1]);
        return -2;
    }

    until((node->attrs & node_attrs::LINK) == 0)
        node = node->child;

    proc->cwd = node;
    return 0;
}

/**
 * Forks a child process to run a command handler. The child invokes fnc and
 * exits with its return value; the parent blocks via waitpid and logs
 * the child's exit status. On fork failure the command is not run.
 *
 * @param fnc:  command handler to run in the child
 * @param argc: number of arguments
 * @param argv: null-terminated argument strings; argv[0] is the command name
 */
@private
fn run_command(fnc: fn (int64, uint8**) -> int64, argc: int64, argv: uint8**) {
    let pid: int64 = fork();
    if (pid < 0) {
        println("[console] fork() returned %d!", pid);
    } else if (pid > 0) {
        let status: int64 = waitpid(pid);
        println("[console] process %d returned %d!", pid, status);
    } else {
        let status: int64 = fnc(argc, argv);
        exit(status);
    }
}

@private
fn try_run(const dir: uint8*, argc: int64, argv: uint8**) -> int32 {
    // open dir
    let dirfd = open(dir, open_mode::DIR);
    if (dirfd < 0) {
        println("open() returned %lld!", dirfd);
        return -1;
    }

    defer close(dirfd);

    // open file
    let fd = openat(dirfd, argv[0], open_mode::READ);
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

    // check for relocation errors
    if (elf_file.relerrs > 0) {
        println("relocation errors: %u", elf_file.relerrs);
        return -7;
    }
    
    // get entry symbol
    let entry = elf64_locate_symbol(&elf_file, "main") as fn (int64, uint8**) -> int64;
    if (entry == null) {
        println("could not find symbol \"main\"!");
        return -8;
    }
    
    // execute
    run_command(entry, argc, argv);
    
    // cleanup
    elf64_unload(&elf_file);

    return 0;
}

/**
 * Dispatches a parsed command. Built-ins run in the console process (cd, which
 * mutates the console's own cwd) or a forked child (ls, cat, mount, help):
 * ls [path] (list a folder's entries), cd <path> (change the working
 * directory), cat <path> (print a file), mount <device> [mountpoint]
 * (fat32_mount; mountpoint defaults to /volumes/<label>), help (list commands).
 * Anything else is treated as an external program: each entry in `dirs` is
 * searched for an ELF object named argv[0] (via try_run), which is loaded,
 * relocated, and run. Unknown commands print a "not found!" message; empty
 * input is ignored.
 *
 * @param argc: number of arguments
 * @param argv: null-terminated argument strings; argv[0] is the command name
 */
@private
fn console_parse_command(argc: int64, argv: uint8**) {
    if (argc == 0)
        return;
    
    if (strcmp(argv[0], "cd") == 0) {
        console_cd(argc, argv);
    } else if (strcmp(argv[0], "ls") == 0) {
        console_ls(argc, argv);
    } else if (strcmp(argv[0], "cat") == 0) {
        run_command(command_cat, argc, argv);
    } else if (strcmp(argv[0], "mount") == 0) {
        run_command(command_mount, argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        run_command(command_help, argc, argv);
    } else {
        let i: uint64 = 0;
        while (i < len(dirs)) {
            if (try_run(dirs[i], argc, argv) == 0)
                return;

            i = i + 1;
        }

        println("command \"%s\" not found!", argv[0]);
    }
}

/**
 * Runs an interactive console loop on pathname. Prompts with the current working directory's name
 * (or "/" at the root) followed by " > ", tokenizes each line into argc/argv (whitespace-delimited;
 * double-quoted strings are treated as single tokens; backslash escapes any character, including \"
 * inside quotes), and dispatches to console_parse_command. Lines
 * with an unterminated quote or trailing backslash are rejected with an error and not dispatched.
 * Does not return.
 *
 * @param pathname: VFS path of the device to use for I/O (e.g. "/dev/serial")
 */
fn console(pathname: uint8*) {
    printk("[console] starting console at \"%s\"...\n", pathname);

    open(pathname, open_mode::READ);
    open(pathname, open_mode::WRITE);

    while (true) {
        let i: uint64;

        let path: uint8* = alloc<uint8*>(1024);
        defer dealloc(path);
        
        getcwd(path, 1024);

        print("%s# ", path);

        let buffer: uint8* = alloc<uint8*>(1024);
        defer dealloc(buffer);

        set_bytes(buffer, 0, 1024);

        let n: uint64 = readline(buffer);
        
        let _quotes = false;
        let _backslash = false;

        let args: struct list<uint8*>;
        list_init(&args, 10);
        defer {
            for arg in &args {
                dealloc(arg);
            }
            list_destroy(&args);
        }

        let vec: struct string;
        string_init(&vec);
        defer string_destroy(&vec);

        i = 0;
        while (i <= n) {
            defer i = i + 1;

            let c: uint8 = buffer[i];
            let arg: uint8*;

            if (_backslash) {
                _backslash = false;
                string_append(&vec, c);
            } else if (_quotes) {
                case (c) {
                when '\\':
                    _backslash = true;
                when '\"':
                    arg = alloc<uint8*>(vec.length + 1);
                    bytecopy(arg, vec.data, vec.length);
                    arg[vec.length] = '\0';

                    list_append(&args, arg);
                    string_reset(&vec);

                    _quotes = false;
                else:
                    string_append(&vec, c);
                }
            } else {
                case (c) {
                when '\\':
                    _backslash = true;
                when '\"':
                    _quotes = true;
                else:
                    if (isspace(c as int32) or c == '\0') {
                        if (vec.length > 0) {
                            arg = alloc<uint8*>(vec.length + 1);
                            bytecopy(arg, vec.data, vec.length);
                            arg[vec.length] = '\0';

                            list_append(&args, arg);
                            string_reset(&vec);
                        }
                    }
                    else {
                        string_append(&vec, c);
                    }
                }
            }
        }

        dprintk("[console] argc=%d, argv=[", args.length);
        for arg in &args {
            dprintk(" \"%s\",", arg);
        }
        dprintk(" ]\n");

        if (_quotes or _backslash)
            dprintk("[console] invalid input!, _quotes=%d, _backslash=%d\n", _quotes, _backslash);
        else
            console_parse_command(args.length as int64, args.data);
    }
}
