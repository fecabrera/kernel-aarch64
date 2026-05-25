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
    . += 0x4000;            /* 16KB stack */
    _stack_top = .;

    . = ALIGN(16);
    _heap_start = .;
    . += 0x400000;          /* 4MB heap */
    _heap_end = .;
}