/*
 * Linker script for the Zircon ARM64 kernel
 * (Samsung Galaxy S9 SM-G960U / Snapdragon SDM845).
 */

ENTRY(_start)

PHDRS
{
    text  PT_LOAD    FLAGS(5);   /* R-X: code + rodata */
    data  PT_LOAD    FLAGS(6);   /* RW-: data + bss   */
    stack PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
    . = 0xFFFF000000000000;

    .text : {
        *(.text.boot)
        *(.text)
        *(.text.*)
    } :text

    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } :text

    . = ALIGN(4096);

    .data : {
        *(.data)
        *(.data.*)
    } :data

    .bss (NOLOAD) : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } :data

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
