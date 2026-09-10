ENTRY(_start)

SECTIONS {
    . = 1M;
    .multiboot2 : { KEEP(*(.multiboot2)) }
    .text   ALIGN(4K) : { *(.text .text.*) }
    .rodata ALIGN(4K) : { *(.rodata .rodata.*) }
    .data   ALIGN(4K) : { *(.data .data.*) }
    
    /* Trampoline section for AP startup - must be at physical address 0x7000 */
    .trampoline 0x7000 : AT(0x7000) {
        _trampoline_start = .;
        *(.trampoline .trampoline.*)
        _trampoline_end = .;
        ASSERT((_trampoline_end - _trampoline_start) <= 0x1000, "Trampoline too large")
    }
    
    .bss    ALIGN(4K) : { *(COMMON) *(.bss .bss.*) }
    _kernel_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
