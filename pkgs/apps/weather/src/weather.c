/* Zircon Weather — a forecast list.  Tap to step through the seeded days. */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    const char *day;
    int high_c;
    int low_c;
    const char *sky;
} forecast_t;

static const forecast_t forecast[] = {
    { "Mon", 22, 14, "Sunny"  },
    { "Tue", 20, 13, "Cloudy" },
    { "Wed", 18, 11, "Rain"   },
    { "Thu", 21, 12, "Sunny"  },
    { "Fri", 23, 15, "Cloudy" },
};
#define FORECAST_DAYS ((int)(sizeof(forecast) / sizeof(forecast[0])))

static int day;

static void weather_render(struct zircapp_app *app, const zircapp_canvas_t *canvas) {
    (void)app;
    if (!canvas || !canvas->fill_rect || !canvas->draw_text)
        return;

    const forecast_t *today = &forecast[day];
    int card_w = canvas->width - 24;
    if (card_w < 1)
        return;

    canvas->fill_rect(canvas->x + 12, canvas->y + 16,
                      card_w, 132, 0xFF24243A);
    canvas->draw_text(canvas->x + 24, canvas->y + 30, 0xFF89B4FA,
                      "Forecast");
    canvas->draw_text(canvas->x + 24, canvas->y + 58, 0xFFCDD6F4,
                      today->day);

    char line[48];
    snprintf(line, sizeof(line), "%s  %d C / %d C",
             today->sky, today->high_c, today->low_c);
    canvas->draw_text(canvas->x + 24, canvas->y + 84, 0xFFCDD6F4, line);
    canvas->draw_text(canvas->x + 24, canvas->y + 116, 0xFFA6ADC8,
                      "Tap upper or lower half to change day");
}

static void weather_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(),
                                    zircapp_phone_get_height(), "Weather");
    day = 0;
    printf("weather: initialized (%d days)\n", FORECAST_DAYS);
}

static void weather_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type != ZIRCAPP_EVENT_TOUCH_DOWN || ev->touch_count <= 0)
        return;
    day = (ev->touch[0].y < zircapp_phone_get_height() / 2)
              ? (day + 1) % FORECAST_DAYS
              : (day - 1 + FORECAST_DAYS) % FORECAST_DAYS;
    printf("weather: %s %d/%d %s\n", forecast[day].day,
           forecast[day].high_c, forecast[day].low_c, forecast[day].sky);
}

static void weather_stop(struct zircapp_app *app) {
    gui_window_close(app->win_id);
}

zircapp_app_t weather_app = {
    .name = "Weather",
    .init = weather_init,
    .event = weather_event,
    .stop = weather_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
    .render = weather_render,
};
