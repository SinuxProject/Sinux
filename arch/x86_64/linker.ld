ENTRY(_start)

SECTIONS {
    /* AP trampoline must be linked below 1 MiB so its 16-bit absolute
       relocations fit. Multiboot2 loads this segment at 0x7000. */
    . = 0x7000;
    .trampoline : AT(0x7000) {
        _trampoline_start = .;
        *(.trampoline .trampoline.*)
        _trampoline_end = .;
    }

    . = 1M;
    .multiboot2 : { KEEP(*(.multiboot2)) }
    .text   ALIGN(4K) : { *(.text .text.*) }
    .rodata ALIGN(4K) : { *(.rodata .rodata.*) }
    .data   ALIGN(4K) : { *(.data .data.*) }
    .bss    ALIGN(4K) : { *(COMMON) *(.bss .bss.*) }
    _kernel_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
