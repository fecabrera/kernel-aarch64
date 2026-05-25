ENTRY(_start)

PHDRS {
    text PT_LOAD FLAGS(5);  /* R-X */
    data PT_LOAD FLAGS(6);  /* RW- */
}

SECTIONS {
    . = 0x40000000;

    .text : {
        *(.text.boot)
        *(.text*)
    } :text

    .rodata : { *(.rodata*) } :text

    . = ALIGN(4096);

    .data : { *(.data*) } :data

    .bss : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } :data

    . = ALIGN(16);
    _stack_bottom = .;
    . += 0x4000;            /* 16KB kernel stack */
    _stack_top = .;

    . = ALIGN(16);
    _irq_stack_bottom = .;
    . += 0x1000;            /* 4KB IRQ stack */
    _irq_stack_top = .;

    . = ALIGN(16);
    _heap_start = .;
    . += 0x400000;          /* 4MB heap */
    _heap_end = .;
}