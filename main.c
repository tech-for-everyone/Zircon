/* Zircon — the Better Android
 * Main entry point. Runs as a userspace service on CodeOS.
 *
 * All app management, the kernel IPC poll, the input poll and the
 * frame loop now live in the ZircApp framework (zircapp_run), so this
 * file is only host bring-up: banner, screen size, GUI stores, register
 * the app tree, and hand control to the framework. */

#include "gui/panels.h"
#include "gui/windows.h"
#include "gui/widgets.h"
#include "zircapp.h"
#include "zircon_abi.h"

#include <stdio.h>

/* ── Built-in app: Home Screen ── */

extern zircapp_app_t home_app;

/* ── Main ── */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("╔══════════════════════════════════════════╗\n");
    printf("║         Zircon OS v1.0.0 (Mobile)          ║\n");
    printf("║   Google-free mobile/desktop OS            ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    /* Set phone screen dimensions from the real framebuffer */
    zircon_fb_info_t fb;
    if (zs_fb_info(&fb) == 0 && fb.width > 0 && fb.height > 0) {
        zircapp_phone_set_screen((int)fb.width, (int)fb.height);
    } else {
        zircapp_phone_set_screen(360, 640);
    }
    gui_desktop_init(zircapp_phone_get_width(), zircapp_phone_get_height());
    gui_window_init();
    zircapp_init();

    /* Register phone apps */
    zircapp_phone_init();
    zircapp_register(&home_app);

    printf("Zircon ready — %d app(s) registered\n", zircapp_count());
    printf("Zircon mobile mode: touch screen %dx%d\n",
           zircapp_phone_get_width(), zircapp_phone_get_height());

    /* Bring up Home and run the ZircApp frame loop. */
    zircapp_launch("Home");
    return zircapp_run(zircapp_find("Home"));
}
