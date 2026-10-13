/*
 * Zircon ARM64 (Samsung Galaxy S9, SM-G960U / Snapdragon SDM845)
 * kernel entry point.
 *
 * boot.S sets up a stack and calls arm64_main() with the boot
 * information pointer that the firmware/bootloader handed us
 * (x0 on entry).
 */

#include <stdint.h>
#include <stddef.h>

/* Qualcomm SDM845 GENI UART (debug console on the S9). */
#define UART0_BASE 0x4a90000

/* Early boot UART byte output — no libc, no interrupts. */
static inline void uart_putc(char c)
{
    volatile uint32_t *uart = (volatile uint32_t *)UART0_BASE;

    /* Wait for TX FIFO not full. */
    while ((uart[0x18 / 4] & 0x20u) == 0)
        ;
    uart[0] = (uint32_t)(uint8_t)c;
}

static void uart_puts(const char *s)
{
    while (*s)
        uart_putc(*s++);
}

static void uart_puthex(uintptr_t v)
{
    static const char digits[] = "0123456789abcdef";
    char buf[16];
    int i;

    for (i = 0; i < 16; i++)
        buf[i] = digits[(v >> ((15 - i) * 4)) & 0xf];
    for (i = 0; i < 16; i++)
        uart_putc(buf[i]);
}

static void banner(void)
{
    uart_puts("\n");
    uart_puts("Zircon OS ARM64 (Samsung Galaxy S9 SM-G960U)\n");
    uart_puts("Built on the CodeOS kernel\n");
    uart_puts("---------------------------------------------\n");
}

/*
 * Called from boot.S.  `boot_info` is either a DTB pointer or the
 * bootloader's boot information structure, depending on who loaded us.
 */
void arm64_main(uintptr_t boot_info)
{
    banner();
    uart_puts("early: console up at 0x");
    uart_puthex(UART0_BASE);
    uart_puts("\n");

    uart_puts("early: boot info @ 0x");
    uart_puthex(boot_info);
    uart_puts("\n");

    uart_puts("early: waiting for full kernel bring-up\n");

    /* No MMU / interrupt controller / scheduler yet: park the core. */
    for (;;)
        __asm__ volatile("wfe");
}
