#ifndef PROCESS_H
#define PROCESS_H

#include <types.h>
#include <string.h>
#include <arch/irq.h>

#define DEFAULT_STACK_SIZE (1 << 14) // 16 KiB

// @todo add support for return values and params
typedef void (*proc_entry)();

typedef enum
{
    PROC_CREATED,
    PROC_READY,
    PROC_RUNNING,
    PROC_BLOCKED,
    PROC_DEAD,
} proc_state_t;

struct process
{
    uint64_t pid;
    proc_state_t state;
    uint8_t *stack;
    struct cpu_context *ctx;
    size_t stack_size;
};

/**
 * Allocates a stack and initializes the process struct with a zeroed context
 * frame. The process is left in PROC_CREATED state; call process_set_entry
 * before enqueueing.
 *
 * @param proc: caller-allocated process struct to initialize
 * @param stack_size: size in bytes of the task stack to allocate
 */
void create_process(struct process *proc, size_t stack_size);

/**
 * Configures the process entry point and initial SPSR.
 * Must be called before the process is enqueued in the scheduler.
 *
 * @param proc: process to configure
 * @param entry: function the process will execute after its first eret
 */
void process_config(struct process *proc, proc_entry entry);

/**
 * Frees the task stack and marks the process as PROC_DEAD.
 * The caller is responsible for removing the process from any run queue
 * before calling this.
 *
 * @param proc: process to destroy
 */
void destroy_process(struct process *proc);

#endif // PROCESS_H