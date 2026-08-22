#include "app.h"
#include "../.gui/windows.h"
#include "../.gui/widgets.h"
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
