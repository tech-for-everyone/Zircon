/* Zircon Clock — Native C clock app */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

static void clock_render(struct zircapp_app *app, const zircapp_canvas_t *canvas) {
    (void)app;
    if (!canvas || !canvas->fill_rect || !canvas->draw_text)
        return;

    int card_w = canvas->width - 32;
    if (card_w < 1)
        return;
    int card_x = canvas->x + 16;
    int card_y = canvas->y + 44;
    canvas->fill_rect(card_x, card_y, card_w, 76, 0xFF24243A);

    char text[32] = "Clock unavailable";
    char *now = ctime(&(time_t){ time(NULL) });
    if (now) {
        size_t len = strlen(now);
        if (len >= sizeof(text))
            len = sizeof(text) - 1;
        memcpy(text, now, len);
        while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'))
            len--;
        text[len] = '\0';
    }
    canvas->draw_text(card_x + 12, card_y + 30, 0xFFCDD6F4, text);
}

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
    .render = clock_render,
};
