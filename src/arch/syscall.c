#include <string.h>
#include <arch/cpu.h>
#include <dsa/set.h>
#include <debug.h>
#include "syscall.h"

static struct set64 _syscall_table;

void syscall_init()
{
    set64_init(&_syscall_table, 10);
}

static interrupt_handler_t _get_syscall_handler(uint64_t syscall_id)
{
    uintptr_t syscall_handler_ptr;
    if (set64_get(&_syscall_table, syscall_id, &syscall_handler_ptr) == 1)
        return (interrupt_handler_t)syscall_handler_ptr;
    else
        return NULL;
}

static void _set_syscall_handler(uint64_t syscall_id, interrupt_handler_t fnc)
{
    set64_set(&_syscall_table, syscall_id, (uintptr_t)fnc);
}

static void _remove_syscall_handler(uint64_t syscall_id)
{
    set64_remove(&_syscall_table, syscall_id);
}

struct cpu_context *syscall_handler(struct cpu_context *ctx)
{
    uint32_t syscall_id = ctx->x0;
    interrupt_handler_t fnc = _get_syscall_handler(syscall_id);

    if (fnc == NULL)
        dprintk("[syscall] Handler not found for syscall %i!\r\n", syscall_id);
    else
        ctx = fnc(ctx);

    return ctx;
}

void syscall_register_handler(uint64_t syscall_id, interrupt_handler_t fnc)
{
    interrupt_handler_t syscall_handler = _get_syscall_handler(syscall_id);
    if (syscall_handler == NULL)
    {
        _set_syscall_handler(syscall_id, fnc);
        dprintk("[syscall] handler registered for syscall %i, addr = 0x%x\r\n", syscall_id, fnc);
    }
    else
    {
        dprintk("[syscall] There's already a handler registered for syscall %i!\r\n", syscall_id);
    }
}

void syscall_unregister_handler(uint64_t syscall_id)
{
    if (_get_syscall_handler(syscall_id) == NULL)
    {
        dprintk("[syscall] There's no handler registered for syscall %i!\r\n", syscall_id);
    }
    else
    {
        _remove_syscall_handler(syscall_id);
        dprintk("[syscall] Handler unregistered for syscall %i!\r\n", syscall_id);
    }
}

int64_t syscall_yield()
{
    int64_t ret;
    __asm__ volatile(
        "mov x0, %1\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"(SYSCALL_YIELD)
        : "x0");
    return ret;
}

void syscall_exit(int64_t status)
{
    __asm__ volatile(
        "mov x0, %0\n"
        "mov x1, %1\n"
        "svc #0"
        :
        : "r"((uint64_t)SYSCALL_EXIT), "r"(status)
        : "x0", "x1");
    halt();
}

pid_t syscall_getpid()
{
    int64_t ret;
    __asm__ volatile(
        "mov x0, %1\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"((uint64_t)SYSCALL_GETPID)
        : "x0");
    return ret;
}

int64_t syscall_waitpid(pid_t pid)
{
    int64_t ret;
    __asm__ volatile(
        "mov x0, %1\n"
        "mov x1, %2\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"((uint64_t)SYSCALL_WAITPID), "r"(pid)
        : "x0", "x1");
    return ret;
}

pid_t syscall_fork()
{
    int64_t ret;
    __asm__ volatile(
        "mov x0, %1\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"((uint64_t)SYSCALL_FORK)
        : "x0");
    return ret;
}

int64_t syscall_sleep(time_t seconds)
{
    int64_t ret;
    __asm__ volatile(
        "mov x0, %1\n"
        "mov x1, %2\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"((uint64_t)SYSCALL_SLEEP), "r"(seconds)
        : "x0", "x1");
    return ret;
}

int64_t syscall_msleep(mseconds_t ms)
{
    int64_t ret;
    __asm__ volatile(
        "mov x0, %1\n"
        "mov x1, %2\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"((uint64_t)SYSCALL_MSLEEP), "r"(ms)
        : "x0", "x1");
    return ret;
}

time_t syscall_time()
{
    time_t ret;
    __asm__ volatile(
        "mov x0, %1\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"((uint64_t)SYSCALL_TIME)
        : "x0");
    return ret;
}

time_t syscall_uptime()
{
    time_t ret;
    __asm__ volatile(
        "mov x0, %1\n"
        "svc #0\n"
        "mov %0, x0"
        : "=r"(ret)
        : "r"((uint64_t)SYSCALL_UPTIME)
        : "x0");
    return ret;
}