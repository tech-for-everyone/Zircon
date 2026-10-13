/* Zircon Clock — Native C clock app */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <time.h>
#include <stdio.h>

static void clock_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(), zircapp_phone_get_height(), "Clock");
    printf("clock: initialized\n");
}

static void clock_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_TICK) {
        time_t now = time(NULL);
        char *t = ctime(&now);
        if (t) printf("clock: %s", t);
    }
}

static void clock_stop(struct zircapp_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircapp_app_t clock_app = {
    .name = "Clock",
    .init = clock_init,
    .event = clock_event,
    .stop = clock_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
