#include <string.h>
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
        uart_puts("[syscall] Handler registered for syscall ");
        uart_put_uint(syscall_id);
        uart_puts("!\r\n");
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

void syscall(uint64_t syscall_id)
{
    __asm__ volatile("mov x0, %0\n svc #0" ::"r"(syscall_id) : "x0");
}