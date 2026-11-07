/*
 * Flat ARM64 Image layout. QEMU virt has RAM at 0x40000000 and the Image
 * header requests the Linux-compatible 0x80000 text offset.
 */
ENTRY(_image_start)

PHDRS
{
    image PT_LOAD FLAGS(7);
    stack PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
    . = 0x40080000;
    __image_start = .;

    .text : ALIGN(16) {
        KEEP(*(.text.boot.header))
        KEEP(*(.text.boot.entry))
        *(.text)
        *(.text.*)
    } :image

    .rodata : ALIGN(16) {
        *(.rodata)
        *(.rodata.*)
    } :image

    .data : ALIGN(4096) {
        *(.data)
        *(.data.*)
    } :image

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss.stack)
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    } :image

    __image_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
