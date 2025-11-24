/* The boot sector jumps to 0x10000; entry.asm must come first in .text. */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    . = 0x10000;
    .text : {
        *(.text.entry)
        *(.text*)
    }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }
    __kernel_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame) }
}
