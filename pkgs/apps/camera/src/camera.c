/* Zircon Camera — Native C camera app */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <string.h>
#include <stdio.h>

static int shots = 0;
static int flash_enabled = 0;

static void camera_render(struct zircapp_app *app, const zircapp_canvas_t *canvas) {
    (void)app;
    if (!canvas || !canvas->fill_rect || !canvas->draw_text ||
        canvas->width < 48 || canvas->height < 120)
        return;

    int preview_y = canvas->y + 12;
    int preview_h = canvas->height - 104;
    int button_y = canvas->y + canvas->height - 68;
    char status[40];

    canvas->fill_rect(canvas->x + 12, preview_y,
                      canvas->width - 24, preview_h, 0xFF0C0C14);
    canvas->fill_rect(canvas->x + 24, preview_y + 12, 32, 3, 0xFF89B4FA);
    canvas->fill_rect(canvas->x + 24, preview_y + 12, 3, 32, 0xFF89B4FA);
    canvas->fill_rect(canvas->x + canvas->width - 56, preview_y + 12,
                      32, 3, 0xFF89B4FA);
    canvas->fill_rect(canvas->x + canvas->width - 27, preview_y + 12,
                      3, 32, 0xFF89B4FA);
    snprintf(status, sizeof(status), "Photos: %d", shots);
    canvas->draw_text(canvas->x + 24, preview_y + 56, 0xFFCDD6F4, status);
    canvas->draw_text(canvas->x + 24, preview_y + 80, 0xFFA6ADC8,
                      flash_enabled ? "Flash on" : "Flash off");
    canvas->fill_rect(canvas->x + canvas->width / 2 - 28, button_y,
                      56, 48, 0xFF89B4FA);
    canvas->draw_text(canvas->x + canvas->width / 2 - 20, button_y + 16,
                      0xFF11111B, "Shoot");
}

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
    .render = camera_render,
};
