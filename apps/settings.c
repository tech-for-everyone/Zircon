/* Zircon Settings — Native C settings app */
#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include <string.h>
#include <stdio.h>

static int wifi_enabled = 1;
static int bluetooth_enabled = 0;
static int screen_brightness = 80;
static int volume = 70;

static void settings_init(struct zircon_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircon_phone_get_width(), zircon_phone_get_height(), "Settings");
    printf("settings: initialized\n");
}

static void settings_event(struct zircon_app *app, const zircon_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCON_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int ty = ev->touch[0].y;
        int h = zircon_phone_get_height();
        if (ty < h / 4) {
            wifi_enabled = !wifi_enabled;
            printf("settings: wifi %s\n", wifi_enabled ? "on" : "off");
        } else if (ty < h / 2) {
            bluetooth_enabled = !bluetooth_enabled;
            printf("settings: bluetooth %s\n", bluetooth_enabled ? "on" : "off");
        } else if (ty < h * 3 / 4) {
            screen_brightness = (screen_brightness + 10) % 101;
            printf("settings: brightness %d\n", screen_brightness);
        } else {
            volume = (volume + 10) % 101;
            printf("settings: volume %d\n", volume);
        }
    }
}

static void settings_stop(struct zircon_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircon_app_t settings_app = {
    .name = "Settings",
    .init = settings_init,
    .event = settings_event,
    .stop = settings_stop,
    .state = ZIRCON_APP_STOPPED,
    .win_id = -1,
};
