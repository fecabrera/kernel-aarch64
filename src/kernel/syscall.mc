import "debug";
import "memory";
import "cpu";
import "set";

const NUM_SYSCALLS = 256;

const SYSCALL_EXIT = 0;
const SYSCALL_YIELD = 1;
const SYSCALL_GETPID = 2;
const SYSCALL_WAITPID = 3;
const SYSCALL_FORK = 4;
const SYSCALL_SLEEP = 5;
const SYSCALL_MSLEEP = 6;
const SYSCALL_TIME = 7;
const SYSCALL_UPTIME = 8;

@static let _syscall_table: struct set<uint64, fn (struct cpu_context *) -> struct cpu_context*>;

/**
 * Initializes the syscall subsystem.
 * Must be called before any syscall_register_handler or syscall_handler invocations. Initializes
 * the internal syscall dispatch table.
 */
fn syscall_init() {
    set_init(&_syscall_table, 10);
}

/**
 * Registers a handler for the given syscall ID.
 *
 * @param syscall_id: syscall number to handle (0–NUM_SYSCALLS-1)
 * @param fnc: function to call when the syscall is invoked
 */
fn syscall_register_handler(syscall_id: uint64, fnc: fn (struct cpu_context*) -> struct cpu_context*) {
    if (get_syscall_handler(syscall_id) == null) {
        set_syscall_handler(syscall_id, fnc);
        dprintk("[syscall] handler registered for syscall %i, addr = %p\n", syscall_id, fnc);
    } else {
        dprintk("[syscall] There's already a handler registered for syscall %i!\n", syscall_id);
    }
}

/**
 * Removes the handler for the given syscall ID.
 * After this call, invoking the syscall will return ctx unchanged.
 *
 * @param syscall_id: syscall number to unregister (0–NUM_SYSCALLS-1)
 */
fn syscall_unregister_handler(syscall_id: uint64) {
    if (get_syscall_handler(syscall_id) == null) {
        dprintk("[syscall] There's no handler registered for syscall %i!\n", syscall_id);
    } else {
        remove_syscall_handler(syscall_id);
        dprintk("[syscall] Handler unregistered for syscall %i!\n", syscall_id);
    }
}

/**
 * Dispatches a syscall based on the number in ctx->x0.
 * Called from sync_handler when ESR_EC_SVC64 is detected.
 *
 * @param ctx: saved context of the calling process
 *
 * @return pointer to the saved cpu_context of the next task to run; may equal ctx if no context
 *         switch is needed.
 */
fn syscall_handler(ctx: struct cpu_context *) -> struct cpu_context* {
    let syscall_id = ctx->x[0];
    let fnc = get_syscall_handler(syscall_id);
    
    if (fnc == null)
        dprintk("[syscall] Handler not found for syscall %i!\n", syscall_id);
    else
        ctx = fnc(ctx);

    return ctx;
}

/**
 * Looks up the handler registered for syscall_id in the dispatch table.
 *
 * @param syscall_id: syscall number to look up
 *
 * @return the registered handler, or null if none is registered
 */
@static
fn get_syscall_handler(syscall_id: uint64) -> fn (struct cpu_context*) -> struct cpu_context* {
    let fnc: fn (struct cpu_context*) -> struct cpu_context*;
    if (!set_get(&_syscall_table, syscall_id, &fnc))
        return null;
    
    return fnc;
}

/**
 * Stores fnc as the handler for syscall_id, overwriting any existing entry.
 *
 * @param syscall_id: syscall number to bind
 * @param fnc:        handler to store
 */
@static
fn set_syscall_handler(syscall_id: uint64, fnc: fn (struct cpu_context*) -> struct cpu_context*) {
    set_set(&_syscall_table, syscall_id, fnc);
}

/**
 * Removes the dispatch-table entry for syscall_id, if any.
 *
 * @param syscall_id: syscall number to clear
 */
@static
fn remove_syscall_handler(syscall_id: uint64) {
    set_remove(&_syscall_table, syscall_id);
}
