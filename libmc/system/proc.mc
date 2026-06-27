// Process identifier (pid_t). A signed integer so that error returns (-1) and
// the valid PID space share one type across the kernel and the syscall ABI.
type proc_pid = int64;
