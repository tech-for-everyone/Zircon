#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include <string.h>

#define ZIRCON_MAX_APPS 32

static zircon_app_t *apps[ZIRCON_MAX_APPS];
static int app_count;

void zircon_app_init(void) {
    app_count = 0;
    for (int i = 0; i < ZIRCON_MAX_APPS; i++)
        apps[i] = 0;
}

int zircon_app_register(zircon_app_t *app) {
    if (!app || app_count >= ZIRCON_MAX_APPS)
        return -1;
    apps[app_count++] = app;
    app->state = ZIRCON_APP_RUNNING;
    if (app->init)
        app->init(app);
    return app_count - 1;
}

int zircon_app_count(void) {
    return app_count;
}

zircon_app_t *zircon_app_get(int idx) {
    if (idx < 0 || idx >= app_count)
        return 0;
    return apps[idx];
}

zircon_app_t *zircon_app_find(const char *name) {
    for (int i = 0; i < app_count; i++)
        if (apps[i] && strcmp(apps[i]->name, name) == 0)
            return apps[i];
    return 0;
}

void zircon_app_broadcast(const zircon_event_t *ev) {
    for (int i = 0; i < app_count; i++) {
        if (!apps[i] || apps[i]->state == ZIRCON_APP_STOPPED)
            continue;
        if (apps[i]->event)
            apps[i]->event(apps[i], ev);
    }
}

void zircon_app_run_all(void) {
    for (int i = 0; i < app_count; i++) {
        if (!apps[i] || apps[i]->state != ZIRCON_APP_RUNNING)
            continue;
        if (apps[i]->event) {
            zircon_event_t tick = { .type = ZIRCON_EVENT_TICK };
            apps[i]->event(apps[i], &tick);
        }
    }
}

int zircon_app_launch(const char *name) {
    zircon_app_t *app = zircon_app_find(name);
    if (!app) return -1;
    app->state = ZIRCON_APP_RUNNING;
    if (app->init)
        app->init(app);
    /* Find the index */
    for (int i = 0; i < app_count; i++)
        if (apps[i] == app) return i;
    return -1;
}

void zircon_app_set_draw_fn(zircon_app_t *app, zircon_draw_fn draw) {
    (void)app;
    (void)draw;
    /* Stores the draw callback for the compositor to use */
}

/* ── Mobile OS Implementation ── */

static zircon_phone_state_t phone_state = ZIRCON_PHONE_IDLE;
static zircon_battery_t battery = { .level = 100, .charging = 0, .plugged = 0 };
static zircon_network_t network = { .signal_strength = 80, .network_type = 4, .wifi_connected = 1 };
static int phone_width = 360;
static int phone_height = 640;

zircon_phone_state_t zircon_phone_get_state(void) { return phone_state; }
void zircon_phone_call(const char *number) {
    (void)number;
    phone_state = ZIRCON_PHONE_CALLING;
}
void zircon_phone_end_call(void) { phone_state = ZIRCON_PHONE_IDLE; }
void zircon_phone_answer(void) { phone_state = ZIRCON_PHONE_IN_CALL; }
void zircon_phone_send_sms(const char *number, const char *text) {
    (void)number; (void)text;
}
zircon_battery_t zircon_battery_get(void) { return battery; }
zircon_network_t zircon_network_get(void) { return network; }
zircon_sensor_data_t zircon_sensor_read(int sensor_type) {
    (void)sensor_type;
    zircon_sensor_data_t s = {0, 0, 0, 0};
    return s;
}
void zircon_phone_init(void) { phone_state = ZIRCON_PHONE_IDLE; }
void zircon_phone_draw_home(void) { }
void zircon_phone_draw_call_screen(const zircon_call_t *call) { (void)call; }
void zircon_phone_draw_sms_screen(void) { }
void zircon_phone_draw_app_drawer(void) { }
void zircon_phone_draw_quick_settings(void) { }
void zircon_touch_process(int count, const int *x, const int *y, const int *id) {
    (void)count; (void)x; (void)y; (void)id;
}
int zircon_phone_get_width(void) { return phone_width; }
int zircon_phone_get_height(void) { return phone_height; }
void zircon_phone_set_screen(int width, int height) {
    phone_width = width;
    phone_height = height;
}
