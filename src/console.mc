import "array";
import "memory";
import "arch/syscall";
import "fs/vfs";

@extern fn isspace(c: uint8) -> int32;
@extern fn atoll(str: uint8*) -> int64;
@extern fn strcmp(lhs: uint8*, rhs: uint8*) -> int32;
@extern fn printk(fmt: uint8*, ...);
@extern fn fat32_mount(pathname: uint8*, mountpoint: uint8*) -> int32;

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
fn _command_help(argc: int64, argv: uint8**) -> int64 {
    printk("available commands:\r\n");

    let i: uint64 = 0;
    while (i < len(available_commands)) {
        let command = available_commands[i];
        printk("    %-28s %s\r\n", command[0], command[1]);
        i = i + 1;
    }
    
    return 0;
}

@private
fn _command_exit(argc: int64, argv: uint8**) -> int64 {
    let status: int64 = 0;
    if (argc > 1) {
        status = atoll(argv[1]);
    }
    printk("exiting with status %d\r\n", status);
    return status;
}

@private
fn _command_mount(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2)
        return -1;

    let moutpoint: uint8* = null;
    if (argc > 2)
        moutpoint = argv[2];

    let status: int64 = fat32_mount(argv[1], moutpoint) as int64;
    if (status < 0) {
        printk("[mount] fat32_mount() returned %d!\r\n", status);
        return -2;
    }

    printk("[mount] block device \"%s\" mounted!\r\n", argv[1]);
    return 0;
}

@private
fn _command_echo(argc: int64, argv: uint8**) -> int64 {
    if (argc > 1)
        printk("%s", argv[1]);
    printk("\r\n");

    return 0;
}

@private
fn _command_cat(argc: int64, argv: uint8**) -> int64 {
    if (argc < 2) {
        printk("no file provided!\r\n");
        return -1;
    }

    let f_size: uint64 = vfs_get_file_size(argv[1]);
    let buffer: uint8* = alloc<uint8>(f_size + 1);

    let status: int32 = vfs_read(argv[1], buffer, f_size, 0);
    if (status < 0) {
        case (status) {
        when -1: // VFS_IO_ERROR_FILE_NOT_FOUND
            printk("file not found!\r\n");
        when -2: // VFS_IO_ERROR_MOUNTPOINT_NOT_FOUND
            printk("mountpoint not found!\r\n");
        when -3: // VFS_IO_ERROR_HANDLER_NOT_PROVIDED
            printk("handler not provided!\r\n");
        else:
            printk("unknown error %d!\r\n", status);
        }
        return -2;
    }

    buffer[f_size] = '\0';
    printk("%s\r\n", buffer);

    dealloc(buffer);

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
 * Dispatches a parsed command by forking a child process. The parent waits via syscall_waitpid and
 * logs the exit status. The child runs the command and exits. Built-in commands: ls (vfs_dump_fs),
 * cat <path> (vfs_read + print), echo [args...] (print first arg),
 * mount <device> [mountpoint] (fat32_mount; mountpoint defaults to /volumes/<label>),
 * exit [status] (syscall_exit), help (list commands). Unknown commands exit silently with 0.
 *
 * @param argc: number of arguments
 * @param argv: null-terminated argument strings; argv[0] is the command name
 */
fn console_parse_command(argc: int64, argv: uint8**) {
    if (argc == 0)
        return;

    let pid: int64 = syscall_fork();
    if (pid < 0) {
        printk("[console] fork() returned %d!\r\n", pid);
        return;
    }

    let status: int64;
    if (pid > 0) {
        status = syscall_waitpid(pid);
        printk("[console] process %d returned %d!\r\n", pid, status);
        return;
    } else {
        status = -1;
        if (strcmp(argv[0], "ls") == 0) {
            vfs_dump_fs();
        } else if (strcmp(argv[0], "exit") == 0) {
            status = _command_exit(argc, argv);
        } else if (strcmp(argv[0], "echo") == 0) {
            status = _command_echo(argc, argv);
        } else if (strcmp(argv[0], "cat") == 0) {
            status = _command_cat(argc, argv);
        } else if (strcmp(argv[0], "mount") == 0) {
            status = _command_mount(argc, argv);
        } else if (strcmp(argv[0], "help") == 0) {
            status = _command_help(argc, argv);
        } else {
            status = 0;
        }

        syscall_exit(status);
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
    while (true) {
        let i: uint64;
        vfs_write(pathname, "> ", 2, 0);

        let buffer: uint8* = alloc<uint8*>(1024);
        set_bytes(buffer, 0, 1024);

        let n: int32 = console_getline(pathname, buffer);
        
        let _quotes = false;
        let _backslash = false;

        let args: array<uint8*>;
        array_init(&args, 10);

        let vec: struct array<uint8>;
        array_init(&vec, 10);

        i = 0;
        while (i <= n as uint64) {
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
                    if (isspace(c) or c == '\0') {
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
            
            i = i + 1;
        }

        array_destroy(&vec);

        printk("[console] argc=%d, argv=[", args.length);
        
        i = 0;
        while (i < args.length) {
            printk(" \"%s\",", args.data[i]);
            i = i + 1;
        }
        printk(" ]\r\n");

        if (_quotes or _backslash)
            printk("[console] invalid input!, _quotes=%d, _backslash=%d\r\n", _quotes, _backslash);
        else
            console_parse_command(args.length as int64, args.data);

        i = 0;
        while (i < args.length) {
            dealloc(args.data[i]);
            i = i + 1;
        }

        array_destroy(&args);
        dealloc(buffer);
    }
}
