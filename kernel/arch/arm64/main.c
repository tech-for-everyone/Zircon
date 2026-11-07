/*
 * Zircon's first runnable AArch64 target is QEMU's ARM virt machine.
 * The existing Galaxy S9 device tree is built separately; its Qualcomm GENI
 * UART and boot chain need board-specific support before that device is
 * claimed as bootable.
 */
#include <stdint.h>
#include <stddef.h>

#define PL011_BASE 0x09000000UL
#define PL011_DR   0x00
#define PL011_FR   0x18
#define PL011_FR_TXFF (1u << 5)
#define UART_WAIT_LIMIT 1000000u

static int uart_putc(char ch)
{
    volatile uint32_t *uart = (volatile uint32_t *)PL011_BASE;
    uint32_t wait = UART_WAIT_LIMIT;
    while ((uart[PL011_FR / sizeof(uint32_t)] & PL011_FR_TXFF) && wait)
        wait--;
    if (!wait)
        return -1;
    uart[PL011_DR / sizeof(uint32_t)] = (uint8_t)ch;
    return 0;
}

static void uart_puts(const char *text)
{
    while (*text) {
        if (*text == '\n')
            uart_putc('\r');
        if (uart_putc(*text++) < 0)
            return;
    }
}

static void uart_puthex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    for (int shift = 60; shift >= 0; shift -= 4)
        uart_putc(digits[(value >> shift) & 0xf]);
}

static uint32_t fdt_be32(const volatile uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) |
           ((uint32_t)bytes[1] << 16) |
           ((uint32_t)bytes[2] << 8) |
           (uint32_t)bytes[3];
}

static int fdt_header_valid(uintptr_t address)
{
    if (!address)
        return 0;
    const volatile uint8_t *fdt = (const volatile uint8_t *)address;
    if (fdt_be32(fdt) != 0xd00dfeed)
        return 0;
    uint32_t total_size = fdt_be32(fdt + 4);
    return total_size >= 40 && total_size <= 16 * 1024 * 1024;
}

void arm64_main(uintptr_t boot_info)
{
    uint64_t current_el;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(current_el));

    uart_puts("\nZircon AArch64 kernel\n");
    uart_puts("CodeOS ARM64 early boot\n");
    uart_puts("early: PL011 console at 0x");
    uart_puthex(PL011_BASE);
    uart_puts("\nearly: CurrentEL=0x");
    uart_puthex(current_el);
    uart_puts("\nearly: FDT pointer=0x");
    uart_puthex(boot_info);
    uart_puts("\n");

    if (!fdt_header_valid(boot_info)) {
        uart_puts("boot error: missing or invalid flattened device tree\n");
        for (;;)
            __asm__ volatile("wfe");
    }

    uart_puts("early: valid device tree, bytes=0x");
    uart_puthex(fdt_be32((const volatile uint8_t *)boot_info + 4));
    uart_puts("\n");
    uart_puts("ZIRCON_ARM64_BOOT_OK\n");
    uart_puts("early: kernel is running; scheduler and device drivers are not initialized\n");

    for (;;)
        __asm__ volatile("wfe");
}
