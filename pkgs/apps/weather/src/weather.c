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
};
