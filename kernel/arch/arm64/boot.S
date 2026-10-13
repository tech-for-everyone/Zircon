/*
 * Zircon ARM64 boot entry point.
 *
 * Limine (aarch64) jumps here with:
 *   x0 = physical address of the Limine boot information structure
 *        (i.e. the kernel command line + boot params), or the DTB pointer.
 *
 * There is no C runtime and no guarantee a stack is set up, so this stub
 * establishes a temporary stack in .bss and calls the C entry point.
 */

.section .text.boot, "ax"
.global _start
.type _start, %function
_start:
    /* Preserve the boot info pointer across the stack setup. */
    mov     x19, x0

    /* Establish the early boot stack. */
    ldr     x1, =__stack_top
    mov     sp, x1

    /* arm64_main(uintptr_t boot_info) */
    mov     x0, x19
    bl      arm64_main

    /* If arm64_main ever returns, park the core. */
1:  wfe
    b       1b

.size _start, . - _start

.section .bss
.align 4
__stack_bottom:
    .skip 65536
__stack_top:
