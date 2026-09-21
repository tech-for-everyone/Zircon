/* Zircon Clock — Native C clock app */
#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include <time.h>
#include <stdio.h>

static void clock_init(struct zircon_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircon_phone_get_width(), zircon_phone_get_height(), "Clock");
    printf("clock: initialized\n");
}

static void clock_event(struct zircon_app *app, const zircon_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCON_EVENT_TICK) {
        time_t now = time(NULL);
        char *t = ctime(&now);
        if (t) printf("clock: %s", t);
    }
}

static void clock_stop(struct zircon_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircon_app_t clock_app = {
    .name = "Clock",
    .init = clock_init,
    .event = clock_event,
    .stop = clock_stop,
    .state = ZIRCON_APP_STOPPED,
    .win_id = -1,
};
