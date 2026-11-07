/* Zircon Gallery — Native C gallery app */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
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

static void gallery_render(struct zircapp_app *app, const zircapp_canvas_t *canvas) {
    (void)app;
    if (!canvas || !canvas->fill_rect || !canvas->draw_text)
        return;

    canvas->fill_rect(canvas->x + 12, canvas->y + 16,
                      canvas->width - 24, canvas->height - 32, 0xFF24243A);
    canvas->draw_text(canvas->x + 28, canvas->y + 36, 0xFF89B4FA,
                      "Gallery");
    if (photo_count == 0) {
        canvas->draw_text(canvas->x + 28, canvas->y + 76, 0xFFCDD6F4,
                          "No photos yet");
        canvas->draw_text(canvas->x + 28, canvas->y + 104, 0xFFA6ADC8,
                          "Captured photos will appear here.");
        return;
    }

    char status[40];
    snprintf(status, sizeof(status), "Photo %d of %d",
             current_photo + 1, photo_count);
    canvas->draw_text(canvas->x + 28, canvas->y + 76, 0xFFCDD6F4, status);
}

static void gallery_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(), zircapp_phone_get_height(), "Gallery");
    photo_count = 0;
    current_photo = 0;
    printf("gallery: initialized\n");
}

static void gallery_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int ty = ev->touch[0].y;
        int h = zircapp_phone_get_height();
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

static void gallery_stop(struct zircapp_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircapp_app_t gallery_app = {
    .name = "Gallery",
    .init = gallery_init,
    .event = gallery_event,
    .stop = gallery_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
    .render = gallery_render,
};
