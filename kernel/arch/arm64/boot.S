/*
 * AArch64 Linux Image-compatible entry header for QEMU virt and bootloaders
 * that load a flat Image at RAM base + text_offset.
 *
 * The first instruction branches over the 64-byte header to _start.
 */
.section .text.boot.header, "ax"
.global _image_start
_image_start:
    b       _start
    .word   0
    .quad   0x80000
    .quad   __image_end - _image_start
    .quad   0
    .quad   0
    .quad   0
    .quad   0
    .word   0x644d5241
    .word   0

.section .text.boot.entry, "ax"
.global _start
.type _start, %function
_start:
    /* QEMU and ARM boot protocols pass the flattened device tree in x0. */
    mov     x19, x0
    msr     daifset, #0xf

    /* Keep this bring-up uniprocessor until secondary-core startup exists. */
    mrs     x1, mpidr_el1
    and     x1, x1, #0xff
    cbnz    x1, 2f

    ldr     x1, =__stack_top
    mov     sp, x1

    /* ELF loaders do not guarantee zeroed BSS. */
    ldr     x1, =__bss_start
    ldr     x2, =__bss_end
1:
    cmp     x1, x2
    b.hs    3f
    str     xzr, [x1], #8
    b       1b
3:
    mov     x0, x19
    bl      arm64_main

2:
    wfe
    b       2b
.size _start, . - _start

.section .bss.stack, "aw", %nobits
.balign 16
__stack_bottom:
    .skip 65536
.global __stack_top
__stack_top:
