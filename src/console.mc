import "debug";
import "std";
import "list";
import "string";
import "memory";
import "scheduler";
import "filesystem/file";
import "filesystem/fs";
import "filesystem/vfs";
import "filesystem/drivers/fat32";
import "libc/ctype";
import "libc/stdlib";
import "system/syscall";

@static
let available_commands: uint8*[][2] = [
    ["ls [path]", "list a folder's entries"],
    ["cd <path>", "change the working directory"],
    ["cat <path>", "print a file"],
    ["stat <path>", "display file status"],
    ["echo [args...]", "print arguments"],
    ["sleep <seconds>", "sleep for the given number of seconds"],
    ["msleep <seconds>", "sleep for the given number of milliseconds"],
    ["mount <device> [mountpoint]", "mount a FAT32 block device"],
    ["exit [status]", "exit with status code"],
    ["help", "show this message"],
];

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
fn command_exit(argc: int64, argv: uint8**) -> int64 {
    let status: int64 = argc > 1 ? atoll(argv[1]) : 0;
    println("exiting with status %lld", status);
    return status;
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
fn command_echo(argc: int64, argv: uint8**) -> int64 {
    println("%s", argc > 1 ? argv[1] : "");
    return 0;
}

@private
fn command_cat(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2) {
        println("usage: %s <path>", argv[0]);
        return -1;
    }

    let fd = open(argv[1], FS_FILE_ATTRS_READ);
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

@private
fn command_ls(argc: int64, argv: uint8**) -> int64 {
    let filename: uint8* = argc > 1 ? argv[1] : "/";
    
    let proc = scheduler_get_current_process();
    
    let node = vfs_get_node_for_path(filename, proc->cwd);
    if (node == null) {
        println("\"%s\" not found!", filename);
        return -1;
    }

    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
        println("\"%s\" is not a folder!", filename);
        return -2;
    }

    until((node->attrs & FS_NODE_ATTRS_FLAG_LINK) == 0)
        node = node->child;

    let current = node->child;
    until(current == null) {
        defer current = current->next;
        if ((current->attrs & FS_NODE_ATTRS_FLAG_HIDDEN) != 0)
            continue;
        
        println("%s", current->name);
    }

    return 0;
}

@private
fn command_stat(argc: int64, argv: uint8**) -> int64 {
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

    if ((st.st_mode & FS_NODE_ATTRS_PERMISSIONS_MASK) > 0) {
        if ((st.st_mode & FS_NODE_ATTRS_PERMISSIONS_READ) > 0) print("r");
        if ((st.st_mode & FS_NODE_ATTRS_PERMISSIONS_WRITE) > 0) print("w");
        if ((st.st_mode & FS_NODE_ATTRS_PERMISSIONS_EXECUTE) > 0) print("x");
        print(" ");
    }
    println("%llu", st.st_size);
    
    return 0;
}

@private
fn command_sleep(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2) {
        println("usage: %s <seconds>", argv[0]);
        return -1;
    }

    let ret = sleep(atoll(argv[1]) as uint64);
    if (ret < 0) {
        println("failed to sleep");
        return ret;
    }

    return 0;
}

@private
fn command_msleep(argc: int64, argv: uint8**) -> int64 {
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

    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
        println("\"%s\" is not a folder!", argv[1]);
        return -2;
    }

    until((node->attrs & FS_NODE_ATTRS_FLAG_LINK) == 0)
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
fn console_run_command(fnc: fn (int64, uint8**) -> int64, argc: int64, argv: uint8**) {
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

/**
 * Dispatches a parsed command to its built-in handler. Most built-ins run in a
 * forked child via console_run_command; cd is the exception, dispatched
 * directly so it can mutate the console process's own cwd. Built-in commands:
 * ls [path] (list a folder's entries), cd <path> (change the working
 * directory), cat <path> (print a file), echo [args...] (print first arg),
 * sleep <seconds> (block for N seconds via the sleep syscall),
 * mount <device> [mountpoint] (fat32_mount; mountpoint defaults to
 * /volumes/<label>), exit [status] (exit), help (list commands).
 * Unknown commands print a "not found!" message. Empty input is ignored.
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
        console_run_command(command_ls, argc, argv);
    } else if (strcmp(argv[0], "exit") == 0) {
        console_run_command(command_exit, argc, argv);
    } else if (strcmp(argv[0], "echo") == 0) {
        console_run_command(command_echo, argc, argv);
    } else if (strcmp(argv[0], "sleep") == 0) {
        console_run_command(command_sleep, argc, argv);
    } else if (strcmp(argv[0], "msleep") == 0) {
        console_run_command(command_msleep, argc, argv);
    } else if (strcmp(argv[0], "cat") == 0) {
        console_run_command(command_cat, argc, argv);
    } else if (strcmp(argv[0], "stat") == 0) {
        console_run_command(command_stat, argc, argv);
    } else if (strcmp(argv[0], "mount") == 0) {
        console_run_command(command_mount, argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        console_run_command(command_help, argc, argv);
    } else {
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

    open(pathname, FS_FILE_ATTRS_READ);
    open(pathname, FS_FILE_ATTRS_WRITE);

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
