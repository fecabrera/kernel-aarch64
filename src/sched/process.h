#pragma once

#include <stdint.h>
#include <string.h>
#include <arch/irq.h>

#define DEFAULT_STACK_SIZE (1 << 14) // 16 KiB

// @todo add support for return values and params
typedef void (*proc_entry)();

typedef enum
{
    PROC_CREATED,
    PROC_READY,
    PROC_BLOCKED,
    PROC_DEAD,
} proc_state_t;

struct process
{
    int64_t pid;
    proc_state_t state;
    uint8_t *stack;          // heap-allocated task stack (base address)
    struct cpu_context *ctx; // saved register frame, embedded near the top of stack
    size_t stack_size;
    uint64_t wait_pid; // PID this process is blocked on; 0 if not waiting
};

/**
 * Allocates a stack and initializes the process struct with a zeroed context
 * frame. The process is left in PROC_CREATED state; call process_config
 * before enqueueing.
 *
 * @param proc: caller-allocated process struct to initialize
 * @param stack_size: size in bytes of the task stack to allocate
 *
 * @return 0 on success, -1 if stack allocation fails
 */
int create_process(struct process *proc, size_t stack_size);

/**
 * Allocates a new stack for dest and copies src's stack contents and context
 * frame into it, preserving the ctx offset within the stack. Assigns dest a
 * new PID and sets its state to PROC_CREATED. Call process_config or adjust
 * dest->ctx->x0 before enqueueing.
 *
 * @param dest: caller-allocated process struct to initialize
 * @param src:  process to copy from
 *
 * @return 0 on success, -1 if stack allocation fails
 */
int duplicate_process(struct process *dest, struct process *src);

/**
 * Configures the process entry point and initial SPSR.
 * Must be called before the process is enqueued in the scheduler.
 *
 * @param proc:  process to configure
 * @param entry: function the process will execute after its first eret
 */
void process_config(struct process *proc, proc_entry entry);

/**
 * Frees the task stack and nulls out stack and ctx pointers.
 * Requires proc->state == PROC_DEAD.
 *
 * @param proc: process to destroy; must be in PROC_DEAD state
 *
 * @return 0 on success, -1 if proc->state != PROC_DEAD
 */
int destroy_process(struct process *proc);
