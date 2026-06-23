#pragma once

// IRQ handler: receives the GIC IRQ ID and the saved register frame.
// Used with irq_register_handler.
typedef struct cpu_context *(*irq_handler_t)(int irq, struct cpu_context *);

// Syscall handler: receives only the saved register frame.
// Used with syscall_register_handler.
typedef struct cpu_context *(*interrupt_handler_t)(struct cpu_context *);

/**
 * Installs the exception vector table by writing its address to vbar_el1,
 * followed by an ISB to ensure the CPU sees the new table immediately.
 * Also initializes the internal set64-backed IRQ dispatch table.
 * Must be called before irq_register_handler or irq_handler.
 */
void irq_init();