/* Zircon Music — Native C music player app */
#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include <string.h>
#include <stdio.h>

static int playing = 0;
static int track = 0;
static int volume = 70;

static void music_init(struct zircon_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircon_phone_get_width(), zircon_phone_get_height(), "Music");
    printf("music: initialized\n");
}

static void music_event(struct zircon_app *app, const zircon_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCON_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int ty = ev->touch[0].y;
        int h = zircon_phone_get_height();
        if (ty < h / 3) {
            playing = !playing;
            printf("music: %s\n", playing ? "playing" : "paused");
        } else if (ty < h * 2 / 3) {
            track = (track + 1) % 10;
            printf("music: track %d\n", track);
        } else {
            volume = (volume + 5) % 101;
            printf("music: volume %d\n", volume);
        }
    }
}

static void music_stop(struct zircon_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircon_app_t music_app = {
    .name = "Music",
    .init = music_init,
    .event = music_event,
    .stop = music_stop,
    .state = ZIRCON_APP_STOPPED,
    .win_id = -1,
};
