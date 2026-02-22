OUTPUT_FORMAT("binary")
ENTRY(_start)

SECTIONS
{
    . = 0x200000;

    .text : {
        *(.text)
        *(.text.*)
        . = ALIGN(4096); /* Force la fin du code sur une page propre */
    }

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        *(.data)
        *(.data.*)
    }

    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}