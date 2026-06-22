/* Zircon — the Better Android
 * Main entry point. Runs as a userspace service on CodeOS,
 * managing apps, the launcher, and the adaptive desktop. */

#include ".gui/panels.h"
#include ".gui/windows.h"
#include ".gui/widgets.h"
#include ".apps/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Built-in app: Launcher ── */

static void launcher_init(zircon_app_t *app) {
    app->win_id = gui_window_create(0, 0, 400, 500, "Zircon Launcher");
}

static void launcher_event(zircon_app_t *app, const zircon_event_t *ev) {
    (void)app;
    (void)ev;
}

static zircon_app_t launcher_app = {
    .name = "Launcher",
    .init = launcher_init,
    .event = launcher_event,
    .stop = 0,
    .state = ZIRCON_APP_RUNNING,
    .win_id = -1,
};

/* ── Main ── */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("Zircon starting...\n");

    gui_desktop_init(1024, 768);
    gui_window_init();
    zircon_app_init();

    /* Register built-in apps */
    zircon_app_register(&launcher_app);

    printf("Zircon ready — %d app(s) registered\n", zircon_app_count());

    zircon_app_launch("Zircon Launcher");

    /* Simple event loop */
    zircon_event_t ev;
    int running = 1;
    while (running) {
        ev.type = ZIRCON_EVENT_TICK;
        zircon_app_broadcast(&ev);
    }

    printf("Zircon shutting down\n");
    return 0;
}
