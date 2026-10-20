/* Zircon Store — the app catalogue for the Zircon phone OS.
 *
 * See src/store.h for what this is and why the state is session-scoped.
 */
#include <zircapp.h>
#include <string.h>
#include <stdio.h>

#include "store.h"

/* The catalogue.
 *
 * The names must match the zircapp registry entries exactly: the Home grid
 * and the store talk about the same apps by name, and a name that is not in
 * the registry is an app that cannot be launched.  home.c is what registers
 * them (pkgs/apps/home/src/home.c), and the compositor draws the screens.
 *
 * The sizes are the on-disk sizes of the package objects as built, rounded
 * up; they are shown in the store so the numbers mean something.
 */
static const store_item_t kCatalogue[] = {
    { "Kall",          "1.0.0", "Phone: dial pad, calls, keypad",     "Phone",    48 },
    { "SMS",           "1.0.0", "Messages inbox and threads",         "Phone",    44 },
    { "Notifications", "1.0.0", "Ongoing alerts and quick replies",   "System",   28 },
    { "Settings",      "1.0.0", "Network, sound and about",           "System",   38 },
    { "About",         "1.0.0", "System information",                 "System",   16 },
    { "Clock",         "1.0.0", "Time, world clock and alarms",       "Tools",    32 },
    { "Calculator",    "1.0.0", "Everyday arithmetic",                "Tools",    24 },
    { "Weather",       "1.0.0", "Forecast for the current location",  "Tools",    36 },
    { "Notes",         "1.0.0", "Quick text notes",                   "Tools",    20 },
    { "Files",         "1.0.0", "Browse the device filesystem",       "Tools",    26 },
    { "Camera",        "1.0.0", "Photo capture",                      "Media",    52 },
    { "Gallery",       "1.0.0", "Browse photos and videos",           "Media",    40 },
    { "Music",         "1.0.0", "Playback controls and library",      "Media",    44 },
    { "Browser",       "1.0.0", "OpenWeb browser",                    "Internet", 64 },
    { "Terminal",      "1.0.0", "Shell access on the device",         "Dev",      30 },
};

#define CATALOGUE_N ((int)(sizeof(kCatalogue) / sizeof(kCatalogue[0])))

/* installed[i] != 0 -> the app is on the Home grid.  Everything starts
 * installed: a phone with an empty home screen is not what anyone wants,
 * and the point of the store is to *remove* things, not to add them. */
static unsigned char installed[STORE_MAX_ITEMS];

static int store_ready;

int store_count(void) {
    return CATALOGUE_N;
}

const store_item_t *store_item(int idx) {
    if (idx < 0 || idx >= CATALOGUE_N) return 0;
    return &kCatalogue[idx];
}

int store_index_of(const char *name) {
    if (!name) return -1;
    for (int i = 0; i < CATALOGUE_N; i++)
        if (!strcmp(kCatalogue[i].name, name)) return i;
    return -1;
}

/* Default state: everything installed.  Called once, before the first read,
 * so a caller never has to wonder whether the table has been initialised. */
static void store_init_once(void) {
    if (store_ready) return;
    for (int i = 0; i < CATALOGUE_N && i < STORE_MAX_ITEMS; i++) installed[i] = 1;
    store_ready = 1;
}

int store_is_installed(const char *name) {
    store_init_once();
    int idx = store_index_of(name);
    /* An app the catalogue does not know about is not managed by the store,
     * so it stays where it is.  Home and Store are themselves such apps: they
     * are always on the grid, otherwise there would be no way back to the
     * grid and no way to undo a removal. */
    if (idx < 0) return 1;
    return installed[idx] ? 1 : 0;
}

int store_set_installed(const char *name, int want) {
    store_init_once();
    int idx = store_index_of(name);
    if (idx < 0) return -1;
    int now = installed[idx] ? 1 : 0;
    installed[idx] = want ? 1 : 0;
    return (now != (want ? 1 : 0)) ? 1 : 0;   /* 1 = the state changed */
}

/* ── The app entry ── */

static void store_init(struct zircapp_app *app) {
    (void)app;
    store_init_once();
    printf("store: catalogue %d apps\n", CATALOGUE_N);
}

static void store_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    (void)ev;
    /* The store's screen is drawn by the compositor (pkgs/core/base/compositor
     * draws every ZircApp screen), so there is nothing to do here: the
     * compositor owns the pixels and calls store_set_installed() when the
     * user taps a row. */
}

static void store_stop(struct zircapp_app *app) {
    (void)app;
}

zircapp_app_t store_app = {
    .name = "Store",
    .init = store_init,
    .event = store_event,
    .stop = store_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
