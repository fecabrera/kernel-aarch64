.section ".text"
.balign 2048                    // MUST be 2KB-aligned!
.global vector_table

vector_table:

// ── EL1t (SP_EL0) ──────────────────────────────────
.balign 128
b       sync_handler_stub       // Synchronous
.balign 128
b       irq_handler_stub        // IRQ
.balign 128
b       fiq_handler_stub        // FIQ
.balign 128
b       serr_handler_stub       // SError

// ── EL1h (SP_EL1) — kernel interrupts ──────────────
.balign 128
b       sync_handler_stub
.balign 128
b       irq_handler_stub        // ← IRQ while in kernel
.balign 128
b       fiq_handler_stub
.balign 128
b       serr_handler_stub

// ── EL0 AArch64 — from userspace ───────────────────
.balign 128
b       sync_handler_stub       // ← syscalls land here
.balign 128
b       irq_handler_stub
.balign 128
b       fiq_handler_stub
.balign 128
b       serr_handler_stub

// ── EL0 AArch32 ────────────────────────────────────
.balign 128
b       sync_handler_stub
.balign 128
b       irq_handler_stub
.balign 128
b       fiq_handler_stub
.balign 128
b       serr_handler_stub


// ── Save/restore macros ─────────────────────────────

.macro save_context
    sub     sp, sp, #272        // Room for all regs
    stp     x0,  x1,  [sp, #0]
    stp     x2,  x3,  [sp, #16]
    stp     x4,  x5,  [sp, #32]
    stp     x6,  x7,  [sp, #48]
    stp     x8,  x9,  [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    mrs     x0, elr_el1         // Return address
    mrs     x1, spsr_el1        // Saved CPU state
    stp     x30, x0,  [sp, #240]
    str     x1,       [sp, #256]
.endm

.macro restore_context
    ldr     x1,        [sp, #256]
    ldp     x30, x0,   [sp, #240]
    msr     elr_el1, x0
    msr     spsr_el1, x1
    ldp     x28, x29,  [sp, #224]
    ldp     x26, x27,  [sp, #208]
    ldp     x24, x25,  [sp, #192]
    ldp     x22, x23,  [sp, #176]
    ldp     x20, x21,  [sp, #160]
    ldp     x18, x19,  [sp, #144]
    ldp     x16, x17,  [sp, #128]
    ldp     x14, x15,  [sp, #112]
    ldp     x12, x13,  [sp, #96]
    ldp     x10, x11,  [sp, #80]
    ldp     x8,  x9,   [sp, #64]
    ldp     x6,  x7,   [sp, #48]
    ldp     x4,  x5,   [sp, #32]
    ldp     x2,  x3,   [sp, #16]
    ldp     x0,  x1,   [sp, #0]
    add     sp, sp, #272
.endm

irq_handler_stub:
    save_context
    mov     x0, sp              // Pass context pointer to C
    bl      irq_handler         // Call C handler
    mov     sp, x0              // switch to new task's stack
    restore_context             // restores new task's registers
    eret                        // Return from exception

sync_handler_stub:
    save_context
    mrs     x0, esr_el1         // Exception Syndrome Register
    mrs     x1, elr_el1         // Faulting address
    mrs     x2, far_el1         // Fault Address Register
    mov     x3, sp
    bl      sync_handler
    restore_context
    eret

fiq_handler_stub:
    save_context
    bl      fiq_handler
    restore_context
    eret

serr_handler_stub:
    save_context
    bl      serr_handler
    restore_context
    eret