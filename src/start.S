.section ".text.boot"
.global _start

_start:
    // Only core 0 runs; park others
    mrs     x1, mpidr_el1
    and     x1, x1, #3
    cbnz    x1, .hang

    // Save DTB
    ldr     x1, =dtb_ptr
    str     x0, [x1]

    // Set up stack (grows downward)
    ldr     x0, =_stack_top
    mov     sp, x0

    // Zero out BSS
    ldr     x0, =_bss_start
    ldr     x1, =_bss_end
    bl      .zero_bss

    // Jump to C
    bl      kernel_init

.hang:
    wfe
    b .hang

.zero_bss:
    cmp     x0, x1
    b.ge    1f
    str     xzr, [x0], #8
    b       .zero_bss
1:  ret

.section ".data"
.global dtb_ptr
dtb_ptr:
    .quad 0