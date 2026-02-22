OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    /* On commence à 1Mo */
    . = 1M;

    _kernel_start = .;

    /* Alignement à 4Ko pour chaque section pour plus de propreté */
    .text BLOCK(4K) : AT(ADDR(.text))
    {
        *(.multiboot) /* Doit être au tout début du binaire */
        *(.text)
    }

    .rodata BLOCK(4K) : AT(ADDR(.rodata))
    {
        *(.rodata)
    }

    .data BLOCK(4K) : AT(ADDR(.data))
    {
        *(.data)
    }

    .bss BLOCK(4K) : AT(ADDR(.bss))
    {
        *(COMMON)
        *(.bss)
    }

    _kernel_end = .;
    _end = .;
    end = .;
}