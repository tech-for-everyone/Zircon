/* Zircon — Minimal Kernel Entry Point
 * Google-free mobile/desktop OS built on CodeOS kernel.
 * This is a minimal entry point for Zircon: no Qt desktop, no network stack,
 * no Android containers, no X11 server. Just the Zircon app manager + compositor. */

#include <stddef.h>
#include <stdint.h>

#include "../arch/x86_64/multiboot.h"
#include "../arch/x86_64/fb.h"
#include "../arch/x86_64/serial.h"
#include "../arch/x86_64/pci.h"
#include "../arch/x86_64/io.h"
#include "../arch/x86_64/rtc.h"
#include "../arch/x86_64/apic.h"

#include "../drivers/input.h"
#include "../drivers/mouse.h"
#include "../drivers/timer.h"
#include "../drivers/drivers.h"
#include "../drivers/zircon_ipc.h"

#include "mm.h"
#include "pmm.h"
#include "vmm.h"
#include "slab.h"
#include "idt.h"
#include "gdt.h"
#include "sched.h"
#include "process.h"
#include "syscall.h"
#include "rng.h"
#include "kprintf.h"
#include "string.h"
#include "version.h"
#include "bootsplash.h"
#include "fs.h"
#include "vfs.h"
#include "ext2.h"
#include "codefs.h"
#include "part.h"
#include "block.h"
#include "elf.h"
#include "umode.h"
#include "script.h"
#include "csl.h"
#include "shell.h"
#include "zircon.h"
#include "display_bridge.h"
#include "audio_mixer.h"
#include "user_wm.h"

extern uint8_t _kernel_start[];
extern uint8_t _kernel_end[];

/* Boot mode flags — set from Limine command line */
int kernel_recovery_mode = 0;
int kernel_safe_mode = 0;

typedef void (*ctor_fn_t)(void);
extern ctor_fn_t __preinit_array_start[];
extern ctor_fn_t __preinit_array_end[];
extern ctor_fn_t __init_array_start[];
extern ctor_fn_t __init_array_end[];

extern void fb_putchar(char);
extern void fb_clear(void);
extern void syscall_entry(void);
extern uint64_t syscall_kernel_rsp;

extern void initramfs_populate(void);
extern int  rootfs_seed_android_stock(void);
extern void fpu_init_early(void);
extern int  limine_booted(void);
extern uint64_t limine_hhdm_offset(void);
extern int  limine_setup_fb(void);
extern void limine_setup_mm(uint64_t *mem_size);
extern const char *limine_get_cmdline(void);

/* ─── Zircon UI pump: drives user_wm_tick + input + cursor in Zircon mode
 *  (no LVGL compositor, so we do it from a kernel thread) ─── */
static void zircon_ui_pump(void) {
    /* Initialize and show mouse cursor */
    extern void fb_cursor_init(void);
    extern void fb_cursor_show(void);
    fb_cursor_init();
    fb_cursor_show();

    for (;;) {
        /* Poll PS/2 + USB input (keyboard + mouse) */
        extern void input_poll(void);
        input_poll();

        /* Update cursor position from mouse */
        extern int mouse_get_x(void);
        extern int mouse_get_y(void);
        extern void fb_cursor_move(int x, int y);
        int mx = mouse_get_x();
        int my = mouse_get_y();
        fb_cursor_move(mx, my);

        /* Route mouse events to compositor */
        extern int mouse_get_buttons(void);
        extern void user_wm_input_pointer(int x, int y, int pressed);
        user_wm_input_pointer(mx, my, mouse_get_buttons() & 1);

        /* Render cursor on top of framebuffer */
        extern void fb_cursor_render(void);
        fb_cursor_render();

        /* Drive user-window bridge (creates/blits/destroys windows) */
        extern void user_wm_tick(void);
        user_wm_tick();

        /* ~60 Hz frame pacing */
        extern void sched_sleep_ms(uint64_t ms);
        sched_sleep_ms(16);
    }
}

static char boot_zircon_init[128];

void kernel_main(uint32_t magic __attribute__((unused)),
                 multiboot_info_t *mb_info) {
    /* Serial first.
     *
     * kprintf() writes through a static buffer and the COM1 port, so it
     * needs neither the PMM nor the slab allocator.  Doing it before
     * anything else means a hang in the memory setup below says so on the
     * serial log instead of leaving an empty log and a guest that merely
     * refuses to reset. */
    serial_init();
    /* kprintf() writes to the serial port only through this hook.  Without
     * it every kprintf() in the kernel goes to the framebuffer (once it is
     * set up) and to nowhere else, so the serial log stays empty for the
     * whole boot -- which is exactly what it did here: the Zircon kernel
     * printed nothing at all, and a hang looked like a silent machine
     * rather than a kernel that had not said a word. */
    kprintf_set_serial(serial_putchar);
    kprintf("\n");
    kprintf("============================================================\n");
    kprintf("  Zircon version %s (x86_64)\n", KERNEL_VERSION_STR);
    kprintf("============================================================\n");

    /* The TSS is allocated from the PMM and lives at a *virtual* address, so
     * both the PMM and phys_to_virt_base have to exist before gdt_tss_init()
     * runs.
     *
     * The CodeOS kernel does both, in this order (kernel/kernel/main.c):
     * phys_to_virt_base = limine_hhdm_offset(); then pmm_init(); then
     * vmm_init(); then idt_init(); then gdt_tss_init().  This entry point
     * had the calls the other way round and never set phys_to_virt_base at
     * all, so gdt_tss_init() computed a NULL TSS pointer and the kernel
     * triple-faulted on its very first instruction -- silently, because
     * serial_init() had not run yet and there was no IDT to report the page
     * fault.  QEMU showed it as a reset (exit 0) with the serial log ending
     * at limine's own ACPI lines.
     */
    if (limine_booted())
        phys_to_virt_base = limine_hhdm_offset();
    kprintf("zircon: hhdm offset 0x%llx\n", (unsigned long long)phys_to_virt_base);

    /* The PMM needs the machine's memory size, not the kernel's own size.
     *
     * pmm_init(size, kernel_start, kernel_end) takes the *total* memory as
     * its first argument and reserves [kernel_start, kernel_end) out of it.
     * This entry point passed the kernel image size instead, so the PMM
     * believed the whole machine was 72 MB -- less than the kernel image --
     * and reserved nothing: "PMM: 18492 pages total (72 MB) ... kernel=... (0
     * pages)".  Every page of the running kernel was then available to the
     * allocator.  limine_setup_mm() is where the real size comes from. */
    uint64_t mem_size = 256ULL * 1024 * 1024;
    if (limine_booted())
        limine_setup_mm(&mem_size);
    pmm_init(mem_size, (uint64_t)_kernel_start, (uint64_t)_kernel_end);
    kprintf("zircon: pmm ok (%llu MB)\n", (unsigned long long)(mem_size >> 20));
    vmm_init();
    kprintf("zircon: vmm ok\n");

    /* The TLS self-pointer: the thread pointer must point at itself so that
     * local-exec TLS accesses land in the TLS block.  boot.S does this too,
     * but before the linker has necessarily resolved __tls_end to a final
     * address; the CodeOS kernel repeats it here for the same reason. */
    {
        extern char __tls_end[];
        *(uint64_t *)__tls_end = (uint64_t)__tls_end;
    }

    /* Initialize CPU, memory, devices */
    idt_init();
    kprintf("zircon: idt ok\n");
    gdt_tss_init();
    kprintf("zircon: gdt ok\n");
    /* The SYSCALL MSRs (LSTAR/STAR/SFMASK).  Without this the `syscall`
     * instruction raises #GP and every userspace program that calls
     * anything -- which is all of them -- dies on its first syscall.  The
     * CodeOS kernel does the same thing in its CPU phase. */
    {
        extern void syscall_entry(void);
        syscall_msr_init(syscall_entry);
    }
    /* syscall_init() installs the int $0x80 handler.  The Zircon libc (and
     * every program cross-compiled against the Zircon target, bas included)
     * enters the kernel with `int $0x80`, so without this every syscall from
     * user mode raises #GP: the IDT entry for vector 0x80 points at nothing.
     * The CodeOS kernel calls it right next to syscall_msr_init(). */
    syscall_init();
    kprintf("zircon: syscall msr ok\n");
    slab_init();
    sched_init();

    /* Framebuffer early */
    limine_setup_fb();
    bootsplash_init();
    fb_clear();
    fb_putchar('Z'); fb_putchar('i'); fb_putchar('r'); fb_putchar('c');
    fb_putchar('o'); fb_putchar('n'); fb_putchar(' '); fb_putchar('O');
    fb_putchar('S'); fb_putchar('\n');

    /* Parse cmdline for zircon.init= */
    const char *cmdline = limine_get_cmdline();
    if (cmdline) {
        const char *p = cmdline;
        while (*p) {
            if (!strncmp(p, "zircon.init=", 12)) {
                p += 12;
                int n = 0;
                while (*p && *p != ' ' && *p != '\t' && n < (int)sizeof(boot_zircon_init) - 1)
                    boot_zircon_init[n++] = *p++;
                boot_zircon_init[n] = '\0';
                break;
            }
            p++;
        }
    }

    /* Core kernel init */
    slab_init();
    rng_init();
    timer_init(1000);
    /* The x87/SSE unit: CR0.MP and CR4.OSFXSR.  Without it an SSE
     * instruction -- in the kernel's own Rust libraries, for instance --
     * raises #UD.  The CodeOS kernel does this in its memory phase. */
    {
        extern void fpu_init_early(void);
        fpu_init_early();
    }
    pci_init();
    drivers_init();
    /* The CMOS RTC.  Without it the clock starts at the epoch, and a program
     * that asks for the time gets 0 -- which is what bas-selftest reported
     * ("time > 0: false") before this was added. */
    rtc_init();
    gdt_tss_init();

    /* Run ctors */
    for (ctor_fn_t *c = __preinit_array_start; c < __preinit_array_end; c++) (*c)();
    for (ctor_fn_t *c = __init_array_start; c < __init_array_end; c++) (*c)();

    /* FS & initramfs */
    /* fs_init() is not optional: it clears the node table and creates the
     * root directory.  Without it every fs_mkdir()/fs_mkfile() fails, and
     * the initramfs population stops at its first directory with
     * "initramfs: cannot create /lib" -- which leaves the image with no
     * userspace programs at all (no /bin/bas-selftest, no /sbin/zircon_init)
     * and nothing but that one line to say so. */
    fs_init();
    vfs_init();
    initramfs_populate();
    block_init();
    part_init();
    int np = part_init();
    for (int i = 0; i < np; i++) {
        partition_t p;
        part_get(i, &p);
        if (p.type == 0x83) {
            if (codefs_mount(i) == 0) {
                script_run_file("/scripts/test1.script");
                script_run_file("/scripts/complex.script");
            } else if (ext2_mount(i) == 0) {
                script_run_file("/scripts/test1.script");
                script_run_file("/scripts/complex.script");
            }
        }
    }

    /* User-window bridge for compositor */
    user_wm_init();

    /* Start Zircon UI pump thread (drives input + user_wm + cursor) */
    extern int sched_create_thread(const char *name, void (*entry)(void));
    sched_create_thread("zircon-ui", zircon_ui_pump);

    /* bas runtime probe: loads and runs /bin/bas-selftest, which exercises
     * the Zircon-target SDK and the freestanding libc.  Run before the
     * compositor so a failure is visible on the serial log rather than being
     * buried under the UI.  The absent case is reported by the probe itself
     * (one line), which keeps a tree without a bas checkout bootable. */
    {
        extern int bas_probe_run(void);
        extern int bas_probe_finished(void);
        bas_probe_run();
        uint64_t bs_start = timer_get_milliseconds();
        while (!bas_probe_finished() &&
               timer_get_milliseconds() - bs_start < 10000)
            sched_yield();
        if (!bas_probe_finished())
            kprintf("BAS: selftest timed out; continuing boot\n");
    }

    /* Initialize Zircon app manager */
    zircon_init();

    /* Boot into Zircon userspace if zircon.init= is set */
    if (boot_zircon_init[0]) {
        bootsplash_finish();
        extern void zircon_launch_elf(const char *path);
        zircon_launch_elf(boot_zircon_init);
    } else {
        /* Fallback: kernel console shell */
        shell_init();
        shell_run();
    }

    /* Should never reach here */
    for (;;) asm volatile("hlt");
}
