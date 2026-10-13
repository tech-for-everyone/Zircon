/* Zircon Camera — Native C camera app */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <string.h>
#include <stdio.h>

static int shots = 0;
static int flash_enabled = 0;

static void camera_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(), zircapp_phone_get_height(), "Camera");
    printf("camera: initialized\n");
}

static void camera_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int ty = ev->touch[0].y;
        int h = zircapp_phone_get_height();
        int w = zircapp_phone_get_width();
        /* Shutter button at bottom center */
        if (ty > h * 0.8 && ty < h * 0.9) {
            shots++;
            printf("camera: captured shot #%d (flash=%s)\n", shots, flash_enabled ? "on" : "off");
        }
        /* Flash toggle */
        if (ty < h * 0.1) {
            flash_enabled = !flash_enabled;
            printf("camera: flash %s\n", flash_enabled ? "on" : "off");
        }
    }
}

static void camera_stop(struct zircapp_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircapp_app_t camera_app = {
    .name = "Camera",
    .init = camera_init,
    .event = camera_event,
    .stop = camera_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
