import "debug";
import "array";
import "memory";
import "syscall";
import "filesystem/fs";
import "filesystem/vfs";
import "filesystem/fat32";
import "libc/ctype";
import "libc/stdlib";
import "libc/string";

@static
let available_commands: uint8*[][2] = [
    ["ls", "list the VFS tree"],
    ["cat <path>", "print a file"],
    ["echo [args...]", "print arguments"],
    ["mount <device> [mountpoint]", "mount a FAT32 block device"],
    ["exit [status]", "exit with status code"],
    ["help", "show this message"],
];

@private
fn command_help(argc: int64, argv: uint8**) -> int64 {
    printk("available commands:\n");

    let i: uint64 = 0;
    while (i < len(available_commands)) {
        defer i = i + 1;
        let command = available_commands[i];
        printk("    %-28s %s\n", command[0], command[1]);
    }
    
    return 0;
}

@private
fn command_ls(argc: int64, argv: uint8**) -> int64 {
    let filename: uint8*;
    let node: struct fs_node*;
    if (argc > 1)
        filename = argv[1];
    else
        filename = "/";
    
    node = vfs_get_node_for_path(filename, null);
    if (node == null) {
        printk("\"%s\" not found!\n", filename);
        return -1;
    }

    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FOLDER) {
        printk("\"%s\" is not a folder!\n", filename);
        return -2;
    }

    until((node->attrs & FS_NODE_ATTRS_FLAG_LINK) == 0)
        node = node->child;

    let current = node->child;
    until(current == null) {
        defer current = current->next;
        if ((current->attrs & FS_NODE_ATTRS_FLAG_HIDDEN) != 0)
            continue;
        
        printk("%s\n", current->name);
    }

    return 0;
}

@private
fn command_exit(argc: int64, argv: uint8**) -> int64 {
    let status: int64 = 0;
    if (argc > 1) {
        status = atoll(argv[1]);
    }
    printk("exiting with status %d\n", status);
    return status;
}

@private
fn command_mount(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2)
        return -1;

    let moutpoint: uint8* = null;
    if (argc > 2)
        moutpoint = argv[2];

    let status: int64 = fat32_mount(argv[1], moutpoint) as int64;
    if (status < 0) {
        printk("fat32_mount() returned %d!\n", status);
        return -2;
    }

    printk("block device \"%s\" mounted!\n", argv[1]);
    return 0;
}

@private
fn command_echo(argc: int64, argv: uint8**) -> int64 {
    if (argc > 1)
        printk("%s", argv[1]);
    printk("\n");

    return 0;
}

@private
fn command_cat(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2) {
        printk("no file provided!\n");
        return -1;
    }
    
    let node = vfs_get_node_for_path(argv[1], null);
    if (node == null) {
        printk("\"%s\" not found!\n", argv[1]);
        return -2;
    }

    if ((node->attrs & FS_NODE_ATTRS_TYPE_MASK) != FS_NODE_ATTRS_TYPE_FILE) {
        printk("\"%s\" is not a file!\n", argv[1]);
        return -3;
    }

    let f_size: uint64 = fs_get_node_file_size(node);
    let buffer: uint8* = alloc<uint8>(f_size + 1);
    defer dealloc(buffer);

    let status: int32 = fs_read(node, buffer, f_size, 0);
    if (status < 0) {
        case (status) {
        when FS_IO_ERROR_FILE_NOT_FOUND:
            printk("file not found!\n");
        when FS_IO_ERROR_NOT_A_FILE:
            printk("not a file!\n");
        when FS_IO_ERROR_MOUNTPOINT_NOT_FOUND:
            printk("mountpoint not found!\n");
        when FS_IO_ERROR_HANDLER_NOT_PROVIDED:
            printk("handler not provided!\n");
        else:
            printk("unknown error %d!\n", status);
        }
        return -4;
    }

    buffer[f_size] = '\0';
    printk("%s\n", buffer);

    return 0;
}

/**
 * Reads the next printable character from the VFS device at pathname, blocking until one arrives.
 * Consumes and discards ANSI escape sequences (ESC [ A/B/C/D arrow keys) transparently.
 *
 * @param pathname: VFS path of the device to read from (e.g. "/dev/serial")
 *
 * @return the next non-escape character read
 */
@private
fn console_getc(pathname: uint8*) -> uint8 {
    let _escape = false;
    let _arrow = false;

    while (true) {
        let c: uint8;
        vfs_read(pathname, &c, 1, 0);

        if (_escape) {
            _escape = false;

            if (c == '[') {
                _arrow = true;
                continue;
            }
        }

        if (_arrow) {
            _arrow = false;
            continue;
        }

        if (c == '\e') { // ASCII_ESC
            _escape = true; // set escape
            continue;
        }

        return c;
    }
    
    return 0;
}

/**
 * Reads one line from the VFS device at pathname into buffer, echoing each character back.
 * Handles escape sequences (arrows silently discarded), CR (echoes newline and returns), DEL
 * (echoes backspace and removes last character), and LF/tab.
 *
 * @param pathname: VFS path of the device to read from (e.g. "/dev/serial")
 * @param buffer:   output buffer for the line (not null-terminated on return)
 *
 * @return number of characters read, excluding the CR terminator
 */
@private
fn console_getline(pathname: uint8*, buffer: uint8*) -> int32 {
    let n: int32 = 0;

    while (true) {
        let c: uint8 = console_getc(pathname);

        case (c) {
        when '\r': // ASCII_CR
            vfs_write(pathname, "\n", 2, 0);
            return n;
        when 0x7F: // ASCII_DEL
            if (n > 0) {
                n = n - 1;
                vfs_write(pathname, "\b \b", 3, 0);
                buffer[n] = '\0';
            }
        else:
            vfs_write(pathname, &c, 1, 0);
            buffer[n] = c;
            n = n + 1;
        }
    }

    return -1;
}

/**
 * Forks a child process to run a command handler. The child invokes fnc and
 * exits with its return value; the parent blocks via syscall_waitpid and logs
 * the child's exit status. On fork failure the command is not run.
 *
 * @param fnc:  command handler to run in the child
 * @param argc: number of arguments
 * @param argv: null-terminated argument strings; argv[0] is the command name
 */
@private
fn console_run_command(fnc: fn (int64, uint8**) -> int64, argc: int64, argv: uint8**) {
    let pid: int64 = syscall_fork();
    if (pid < 0) {
        printk("[console] fork() returned %d!\n", pid);
    } else if (pid > 0) {
        let status: int64 = syscall_waitpid(pid);
        printk("[console] process %d returned %d!\n", pid, status);
    } else {
        let status: int64 = fnc(argc, argv);
        syscall_exit(status);
    }
}

/**
 * Dispatches a parsed command to its built-in handler, each run in a forked
 * child via console_run_command. Built-in commands: ls [path] (list a folder's
 * entries), cat <path> (print a file), echo [args...] (print first arg),
 * mount <device> [mountpoint] (fat32_mount; mountpoint defaults to
 * /volumes/<label>), exit [status] (syscall_exit), help (list commands).
 * Unknown commands print a "not found!" message. Empty input is ignored.
 *
 * @param argc: number of arguments
 * @param argv: null-terminated argument strings; argv[0] is the command name
 */
@private
fn console_parse_command(argc: int64, argv: uint8**) {
    if (argc == 0)
        return;
    
    if (strcmp(argv[0], "ls") == 0) {
        console_run_command(command_ls, argc, argv);
    } else if (strcmp(argv[0], "exit") == 0) {
        console_run_command(command_exit, argc, argv);
    } else if (strcmp(argv[0], "echo") == 0) {
        console_run_command(command_echo, argc, argv);
    } else if (strcmp(argv[0], "cat") == 0) {
        console_run_command(command_cat, argc, argv);
    } else if (strcmp(argv[0], "mount") == 0) {
        console_run_command(command_mount, argc, argv);
    } else if (strcmp(argv[0], "help") == 0) {
        console_run_command(command_help, argc, argv);
    } else {
        printk("command \"%s\" not found!\n", argv[0]);
    }
}

/**
 * Runs an interactive console loop on pathname. Prompts with "> ", tokenizes each line into
 * argc/argv (whitespace-delimited; double-quoted strings are treated as single tokens; backslash
 * escapes any character, including \" inside quotes), and dispatches to console_parse_command. Lines
 * with an unterminated quote or trailing backslash are rejected with an error and not dispatched.
 * Does not return.
 *
 * @param pathname: VFS path of the device to use for I/O (e.g. "/dev/serial")
 */
fn console(pathname: uint8*) {
    printk("[console] starting console at \"%s\"...\n", pathname);
    while (true) {
        let i: uint64;

        vfs_write(pathname, "> ", 2, 0);

        let buffer: uint8* = alloc<uint8*>(1024);
        defer dealloc(buffer);

        set_bytes(buffer, 0, 1024);

        let n: int32 = console_getline(pathname, buffer);
        
        let _quotes = false;
        let _backslash = false;

        let args: array<uint8*>;
        array_init(&args, 10);
        defer {
            for arg in &args {
                dealloc(arg);
            }
            array_destroy(&args);
        }

        let vec: struct array<uint8>;
        array_init(&vec, 10);
        defer array_destroy(&vec);

        i = 0;
        while (i <= n as uint64) {
            defer i = i + 1;

            let c: uint8 = buffer[i];
            let arg: uint8*;

            if (_backslash) {
                _backslash = false;
                array_append(&vec, c);
            } else if (_quotes) {
                case (c) {
                when '\\':
                    _backslash = true;
                when '\"':
                    arg = alloc<uint8*>(vec.length + 1);
                    copy_bytes(arg, vec.data, vec.length);
                    arg[vec.length] = '\0';

                    array_append(&args, arg);
                    array_reset(&vec);

                    _quotes = false;
                else:
                    array_append(&vec, c);
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
                            copy_bytes(arg, vec.data, vec.length);
                            arg[vec.length] = '\0';

                            array_append(&args, arg);
                            array_reset(&vec);
                        }
                    }
                    else {
                        array_append(&vec, c);
                    }
                }
            }
        }

        printk("[console] argc=%d, argv=[", args.length);
        
        for arg in &args {
            printk(" \"%s\",", arg);
        }
        printk(" ]\n");

        if (_quotes or _backslash)
            printk("[console] invalid input!, _quotes=%d, _backslash=%d\n", _quotes, _backslash);
        else
            console_parse_command(args.length as int64, args.data);
    }
}
