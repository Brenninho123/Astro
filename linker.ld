/* Astro OS - linker script (32-bit x86, loaded at 1 MiB by a Multiboot bootloader) */
ENTRY(_start)

SECTIONS
{
    . = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        KEEP(*(.multiboot))
        *(.text)
    }

    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata*)
    }

    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
    }

    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
    }

    /* Toolchain metadata we do not need; keeps the layout predictable. */
    /DISCARD/ :
    {
        *(.note.gnu.property)
        *(.note.gnu.build-id)
        *(.eh_frame)
        *(.comment)
    }
}
