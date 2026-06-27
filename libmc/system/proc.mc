// Process identifier (pid_t). A signed integer so that error returns (-1) and
// the valid PID space share one type across the kernel and the syscall ABI.
type proc_pid = int64;

// Failure reasons returned by the exec/execat syscalls (and process_exec*). 0 is
// success; on success the call does not return (the new program takes over), so
// these negative values are only ever observed on failure.
enum exec_err: int64 {
    FILE_NOT_FOUND       = -1, // path did not resolve to a file
    NOT_ENOUGH_MEMORY    = -2, // could not allocate the read buffer
    IO_ERROR             = -3, // reading the executable failed
    INVALID_ELF          = -4, // not a valid ELF object
    PARSING_ERROR        = -5, // elf_read rejected the object
    CANNOT_REPLACE_IMAGE = -6, // could not allocate the new process image
    RELOCATION_ERROR     = -7, // one or more relocations failed
    ENTRY_NOT_FOUND      = -8, // the "entry" symbol was missing
}
