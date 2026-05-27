#include <string.h>
#include <arch/cpu.h>
#include <drivers/uart.h>
#include "syscall.h"

interrupt_handler syscall_table[NUM_SYSCALLS] = {NULL};

struct cpu_context *syscall_handler(struct cpu_context *ctx)
{
    uint32_t syscall_id = ctx->x0;
    interrupt_handler fnc = syscall_table[syscall_id];

    if (fnc == NULL)
    {
        uart_puts("[syscall] Handler not found for syscall ");
        uart_put_uint(syscall_id);
        uart_puts("!\r\n");
    }
    else
    {
        ctx = fnc(ctx);
    }

    return ctx;
}

void syscall_register_handler(uint64_t syscall_id, interrupt_handler fnc)
{
    if (syscall_table[syscall_id] == NULL)
    {
        syscall_table[syscall_id] = fnc;
        uart_puts("[syscall] handler registered for syscall ");
        uart_put_uint(syscall_id);
        uart_puts(", addr = 0x");
        uart_put_uint_hex((uintptr_t)fnc);
        uart_puts("\r\n");
    }
    else
    {
        uart_puts("[syscall] There's already a handler registered for syscall ");
        uart_put_uint(syscall_id);
        uart_puts("!\r\n");
    }
}

void syscall_unregister_handler(uint64_t syscall_id)
{
    if (syscall_table[syscall_id] == NULL)
    {
        uart_puts("[syscall] There's no handler registered for syscall ");
        uart_put_uint(syscall_id);
        uart_puts("!\r\n");
    }
    else
    {
        syscall_table[syscall_id] = NULL;
        uart_puts("[syscall] Handler unregistered for syscall ");
        uart_put_uint(syscall_id);
        uart_puts("!\r\n");
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

void syscall_exit(uint64_t status)
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

uint64_t syscall_getpid()
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

uint64_t syscall_waitpid(uint64_t pid)
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