#include <mm/heap.h>
#include <drivers/uart.h>
#include "scheduler.h"

struct scheduler_entry *current_entry = NULL;

void scheduler_enqueue(struct process *proc)
{
    proc->state = PROC_READY;

    struct scheduler_entry *entry = (struct scheduler_entry *)kmalloc(sizeof(struct scheduler_entry));
    entry->proc = proc;

    if (current_entry == NULL)
    {
        entry->prev = entry;
        entry->next = entry;
        current_entry = entry;
    }
    else
    {
        entry->prev = current_entry->prev;
        entry->next = current_entry;

        current_entry->prev->next = entry;
        current_entry->prev = entry;
    }

    uart_puts("[scheduler] pid ");
    uart_put_uint(proc->pid);
    uart_puts(" queued up\r\n");
}

struct cpu_context *scheduler_handler(struct cpu_context *ctx)
{
    if (current_entry == NULL)
    {
        // if there are no processes, return the current ctx
        return ctx;
    }

    struct scheduler_entry *next_entry = current_entry;

    do
    {
        // get next process
        next_entry = next_entry->next;

        if (next_entry == current_entry)
        {
            // if no other process is ready, return the current ctx
            current_entry->proc->state = PROC_RUNNING;
            return current_entry->proc->ctx;
        }
    } while (next_entry->proc->state != PROC_READY);

    // update statuses
    current_entry->proc->state = PROC_READY;
    next_entry->proc->state = PROC_RUNNING;

    // update entries
    current_entry = next_entry;

    // return ctx
    return current_entry->proc->ctx;
}