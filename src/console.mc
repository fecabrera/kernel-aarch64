import "debug";
import "std";
import "list";
import "string";
import "memory";
import "range";
import "scheduler";
import "filesystem/file";
import "filesystem/fs";
import "filesystem/vfs";
import "filesystem/drivers/fat32";
import "libc/ctype";
import "libc/stdlib";
import "system/syscall";

@static
let available_commands: char*[][2] = [
    ["ls [path]", "list a folder's entries"],
    ["cd <path>", "change the working directory"],
    ["mount <device> [mountpoint]", "mount a FAT32 block device"],
    ["help", "show this message"],
];

@static
let dirs: char*[] = ["/bin"];

/**
 * Prints the built-in command list (from the available_commands table) to the
 * console.
 *
 * @param argc: number of arguments (unused)
 * @param argv: argument vector (unused)
 *
 * @return 0
 */
@private
fn console_help(argc: int64, argv: char**) -> int64 {
    println("built-in commands:");

    let r = struct range<int32> { end = len(available_commands) };
    for i in &r {
        let command = available_commands[i];
        println("    %-28s %s", command[0], command[1]);
    }
    
    return 0;
}

/**
 * Mounts the FAT32 block device named by argv[1] via fat32_mount. An optional
 * argv[2] overrides the mountpoint; when omitted it defaults to the device's
 * label (as chosen by fat32_mount).
 *
 * @param argc: number of arguments
 * @param argv: argv[1] is the block device path; argv[2] is the optional mountpoint
 *
 * @return 0 on success, 1 if no device was given, 2 if fat32_mount fails
 */
@private
fn console_mount(argc: int64, argv: char**) -> int64 {
    if (argc < 2) {
        println("usage: %s <device> [mountpoint]", argv[0]);
        return 1;
    }

    let mountpoint: char* = argc > 2 ? argv[2] : null;

    let status = fat32_mount(argv[1], mountpoint);
    if (status < 0) {
        println("fat32_mount() returned %d!", status);
        return 2;
    }

    println("block device \"%s\" mounted!", argv[1]);
    return 0;
}

/**
 * Lists the entries of the folder at argv[1] (defaulting to "/" when no path is
 * given), resolved absolutely from the VFS root or relative to the process cwd.
 * Prints each child's name, skipping entries flagged node_attrs::HIDDEN (e.g.
 * "." and "..").
 *
 * @param argc: number of arguments
 * @param argv: argv[1] is the optional folder path
 *
 * @return 0 on success, 1 if the path is not found or is not a folder
 */
@private
fn console_ls(argc: int64, argv: char**) -> int64 {
    let filename: char* = argc > 1 ? argv[1] : "/";
    
    let proc = scheduler_get_current_process();
    
    let root = filename[0] == '/' ? vfs_root() : proc->cwd;
    let node = vfs_get_node_for_path(filename, root);
    if (node == null) {
        println("\"%s\" not found!", filename);
        return 1;
    }

    if (fs_node_get_type(node) != node_attrs::DIR) {
        println("\"%s\" is not a folder!", filename);
        return 1;
    }

    let current = node->child;
    until (current == null) {
        defer current = current->next;
        if (fs_node_test_attribute(current, node_attrs::HIDDEN))
            continue;
        
        println("%s", current->name);
    }

    return 0;
}

/**
 * Changes the calling process's current working directory to argv[1] via the
 * chdir syscall (which resolves the path absolutely from the VFS root or relative
 * to the cwd, and requires it to name a folder). Like the other built-ins, cd
 * runs directly in the console process rather than a forked child, so the cwd
 * update lands in the shell itself.
 *
 * @param argc: number of arguments
 * @param argv: argv[0] is the command name; argv[1] is the destination path
 *
 * @return 0 on success, -1 if no path was given, 1 if chdir fails (path not found
 *         or not a folder)
 */
@private
fn console_cd(argc: int64, argv: char**) -> int64 {
    if (argc < 2) {
        println("usage: %s <path>", argv[0]);
        return -1;
    }

    let status = chdir(argv[1]);
    if (status < 0) {
        println("chdir() returned %lld", status);
        return 1;
    }

    return 0;
}

/**
 * Attempts to run argv[0] as a program found in directory `dir`. Opens `dir` and
 * uses statat to check the program exists there *before* forking, so a missing
 * program returns -2 (letting console_parse_command try the next search dir)
 * without spawning a process. If it exists, forks: the child execats it (and on
 * any exec failure exits, so it never falls back into the shell loop), while the
 * parent waitpids and reports the exit status.
 *
 * @param dir:  directory to look for the program in (a `dirs` search-path entry)
 * @param argc: number of arguments
 * @param argv: argument vector; argv[0] is the program name
 *
 * @return 0 if the program was found and a child was forked/waited on, -1 if the
 *         directory could not be opened, -2 if the program is not in `dir`, -3 if
 *         fork failed
 */
@private
fn try_run(const dir: char*, argc: int64, argv: char**) -> int64 {
    // open dir
    let dirfd = open(dir, open_mode::DIR);
    if (dirfd < 0) {
        println("[console] open() returned %lld!", dirfd);
        return -1;
    }

    defer close(dirfd);

    let st: struct file_stat;
    let st_status = statat(dirfd, argv[0], &st);
    if (st_status < 0)
        return -2;
    
    let pid: int64 = fork();
    if (pid < 0) {
        println("[console] fork() returned %d!", pid);
        return -3;
    }
    
    if (pid > 0) {
        let status: int64 = waitpid(pid);
        println("[console] process %d returned %d!", pid, status);
    } else {
        exit(execat(dirfd, argv[0], argc, argv));
    }

    return 0;
}

/**
 * Dispatches a parsed command. Built-ins all run directly in the console process:
 * ls [path] (list a folder's entries), cd <path> (change the working directory),
 * mount <device> [mountpoint] (fat32_mount; mountpoint defaults to the device
 * label), help (list commands). Anything else is treated as an external program:
 * each entry in `dirs` is searched for an ELF object named argv[0] (via try_run),
 * which is fork+execat'd into its own process. Unknown commands print a
 * "not found!" message; empty input is ignored.
 *
 * @param argc: number of arguments
 * @param argv: null-terminated argument strings; argv[0] is the command name
 */
@private
fn console_parse_command(argc: int64, argv: char**) {
    if (argc == 0)
        return;

    case (0) {
    when strcmp(argv[0], "cd"):
        console_cd(argc, argv);
    when strcmp(argv[0], "ls"):
        console_ls(argc, argv);
    when strcmp(argv[0], "mount"):
        console_mount(argc, argv);
    when strcmp(argv[0], "help"):
        console_help(argc, argv);
    else:
        let r = struct range<int32> { end = len(dirs) };
        for i in &r {
            if (try_run(dirs[i], argc, argv) == 0)
                return;
        }

        println("command \"%s\" not found!", argv[0]);
    }
}

/**
 * Runs the interactive console loop. Prompts with the absolute path of the
 * current working directory (via getcwd) followed by "# ", reads a line, and
 * tokenizes it into argc/argv (whitespace-delimited; double-quoted strings are
 * treated as single tokens; backslash escapes any character, including \" inside
 * quotes) before dispatching to console_parse_command. Lines with an unterminated
 * quote or trailing backslash are rejected with an error and not dispatched.
 * Does not return.
 */
fn console() {
    while (true) {
        let i: uint64;

        let path: char* = alloc<char>(1024);
        defer dealloc(path);
        
        getcwd(path, 1024);

        print("%s# ", path);

        let buffer: char* = alloc<char>(1024);
        defer dealloc(buffer);

        set_bytes(buffer, 0, 1024);

        let n: uint64 = readline(buffer);
        
        let _quotes = false;
        let _backslash = false;

        let args: struct list<char*>;
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

        let r = struct range { end = n + 1 };
        for i in &r {
            let c: char = buffer[i];
            let arg: char*;

            if (_backslash) {
                _backslash = false;
                case (c) {
                when 'a':
                    string_push(&vec, '\a');
                when 'b':
                    string_push(&vec, '\b');
                when 't':
                    string_push(&vec, '\t');
                when 'n':
                    string_push(&vec, '\n');
                when 'f':
                    string_push(&vec, '\f');
                when 'r':
                    string_push(&vec, '\r');
                when 'e':
                    string_push(&vec, '\e');
                else:
                    string_push(&vec, c);
                }
            } else if (_quotes) {
                case (c) {
                when '\\':
                    _backslash = true;
                when '\"':
                    arg = alloc<char>(vec.length + 1);
                    bytecopy(arg, vec.data, vec.length);
                    arg[vec.length] = '\0';

                    list_push(&args, arg);
                    string_reset(&vec);

                    _quotes = false;
                else:
                    string_push(&vec, c);
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
                            arg = alloc<char>(vec.length + 1);
                            bytecopy(arg, vec.data, vec.length);
                            arg[vec.length] = '\0';

                            list_push(&args, arg);
                            string_reset(&vec);
                        }
                    }
                    else {
                        string_push(&vec, c);
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
