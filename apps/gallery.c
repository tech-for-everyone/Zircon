/* Zircon Gallery — Native C gallery app */
#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include <string.h>
#include <stdio.h>

#define MAX_PHOTOS 100

typedef struct {
    char name[64];
    int width, height;
} photo_t;

static photo_t photos[MAX_PHOTOS];
static int photo_count;
static int current_photo;

static void gallery_init(struct zircon_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircon_phone_get_width(), zircon_phone_get_height(), "Gallery");
    photo_count = 0;
    current_photo = 0;
    printf("gallery: initialized\n");
}

static void gallery_event(struct zircon_app *app, const zircon_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCON_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int ty = ev->touch[0].y;
        int h = zircon_phone_get_height();
        if (ty < h / 2) {
            /* Next photo */
            if (photo_count > 0) {
                current_photo = (current_photo + 1) % photo_count;
                printf("gallery: photo %d\n", current_photo);
            }
        } else {
            /* Previous photo */
            if (photo_count > 0) {
                current_photo = (current_photo - 1 + photo_count) % photo_count;
                printf("gallery: photo %d\n", current_photo);
            }
        }
    }
}

static void gallery_stop(struct zircon_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircon_app_t gallery_app = {
    .name = "Gallery",
    .init = gallery_init,
    .event = gallery_event,
    .stop = gallery_stop,
    .state = ZIRCON_APP_STOPPED,
    .win_id = -1,
};
