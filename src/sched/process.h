#pragma once

#include <arch/irq.h>
#include <fs/filesystem.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define DEFAULT_STACK_SIZE (1 << 14) // 16 KiB

// @todo add support for return values and params
typedef void (*proc_entry)();

#define PROC_DEAD -1
#define PROC_CREATED 0
#define PROC_READY 1
#define PROC_BLOCKED 2

typedef int64_t proc_state_t;

/**
 * Represents a schedulable process. Created by create_process, configured by
 * process_set_entry, and destroyed by destroy_process.
 *
 * @field pid:         (pid_t) unique process identifier, assigned at creation
 * @field state:       current lifecycle state (PROC_CREATED, PROC_READY, PROC_BLOCKED, or
 *                     PROC_DEAD)
 * @field stack:       base address of the heap-allocated task stack
 * @field ctx:         saved register frame (cpu_context), embedded near the top of stack and
 *                     updated on every context switch
 * @field stack_size:  size of the stack allocation in bytes
 * @field wait_pid:    (pid_t) PID this process is blocked waiting for via waitpid; 0 when not
 *                     waiting
 * @field sleep_until: (time_t) absolute uptime in milliseconds at which the process should wake; 0
 *                     when not sleeping
 * @field cwd:         current working directory node; inherited from the parent on fork
 */
struct process {
    pid_t pid;
    proc_state_t state;
    uint8_t *stack;
    struct cpu_context *ctx;
    size_t stack_size;
    pid_t wait_pid;
    time_t sleep_until;
    struct fs_node *cwd;
};

/**
 * Allocates a stack and initializes the process struct with a zeroed context
 * frame. The current working directory defaults to the VFS root. The process
 * is left in PROC_CREATED state; call process_set_entry before enqueueing.
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
 * new PID, sets its state to PROC_CREATED, and inherits src's current working
 * directory. Call process_set_entry or adjust dest->ctx->x0 before enqueueing.
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
void process_set_entry(struct process *proc, proc_entry entry);

/**
 * Frees the task stack and nulls out stack and ctx pointers.
 * Requires proc->state == PROC_DEAD.
 *
 * @param proc: process to destroy; must be in PROC_DEAD state
 *
 * @return 0 on success, -1 if proc->state != PROC_DEAD
 */
int destroy_process(struct process *proc);
