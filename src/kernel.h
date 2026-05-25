#ifndef KERNEL_H
#define KERNEL_H

/**
 * Entry point for the kernel process. Runs as a scheduled task after
 * the kernel is fully initialized.
 */
void kernel_proc();

/**
 * Kernel entry point. Initializes all subsystems and starts the scheduler.
 * Called from start.S after BSS is zeroed and the stack is set up.
 */
void kernel_init();

#endif // KERNEL_H