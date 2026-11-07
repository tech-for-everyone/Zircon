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
    { "Kall",          "1.0.0", "Phone: dial pad, calls, keypad",     "Phone",    "phone",    48 },
    { "SMS",           "1.0.0", "Messages inbox and threads",         "Phone",    "phone",    44 },
    { "Notifications", "1.0.0", "Ongoing alerts and quick replies",   "System",   "notifications", 28 },
    { "Settings",      "1.0.0", "Network, sound and about",           "System",   "system",   38 },
    { "About",         "1.0.0", "System information",                 "System",   "system",   16 },
    { "Clock",         "1.0.0", "Time, world clock and alarms",       "Tools",    "time",     32 },
    { "Calculator",    "1.0.0", "Everyday arithmetic",                "Tools",    "none",     24 },
    { "Weather",       "1.0.0", "Forecast for the current location",  "Tools",    "network",  36 },
    { "Notes",         "1.0.0", "Quick text notes",                   "Tools",    "files.read, files.write", 20 },
    { "Files",         "1.0.0", "Browse the device filesystem",       "Tools",    "files.read, files.write", 26 },
    { "Camera",        "1.0.0", "Photo capture",                      "Media",    "camera",   52 },
    { "Gallery",       "1.0.0", "Browse photos and videos",           "Media",    "files.read", 40 },
    { "Music",         "1.0.0", "Playback controls and library",      "Media",    "audio, files.read", 44 },
    { "Browser",       "1.0.0", "OpenWeb browser",                    "Internet", "network",  64 },
    { "Terminal",      "1.0.0", "Shell access on the device",         "Dev",      "system",   30 },
    { "Move On",       "1.0.0", "Prepare user files for CodeOS",       "Tools",    "files.read, files.write", 24 },
};

#define CATALOGUE_N ((int)(sizeof(kCatalogue) / sizeof(kCatalogue[0])))
#define CATEGORY_N 7

static const char *const kCategories[CATEGORY_N] = {
    "All", "Phone", "System", "Tools", "Media", "Internet", "Dev"
};

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

static int equal_folded(const char *left, const char *right) {
    if (!left || !right) return 0;
    while (*left && *right) {
        char a = *left++;
        char b = *right++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return 0;
    }
    return *left == 0 && *right == 0;
}

static int contains_folded(const char *text, const char *query) {
    if (!query || !query[0]) return 1;
    if (!text) return 0;
    for (const char *start = text; *start; start++) {
        const char *a = start;
        const char *b = query;
        while (*a && *b) {
            char ca = *a++;
            char cb = *b++;
            if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
            if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
            if (ca != cb) break;
        }
        if (!*b) return 1;
    }
    return 0;
}

int store_category_count(void) {
    return CATEGORY_N;
}

const char *store_category(int idx) {
    if (idx < 0 || idx >= CATEGORY_N) return 0;
    return kCategories[idx];
}

static int matches_filter(const store_item_t *item, const char *query,
                          const char *category) {
    if (!item) return 0;
    if (category && category[0] && !equal_folded(category, "All") &&
        !equal_folded(item->category, category))
        return 0;
    return contains_folded(item->name, query) ||
           contains_folded(item->desc, query) ||
           contains_folded(item->category, query);
}

int store_filtered_count(const char *query, const char *category) {
    int count = 0;
    for (int i = 0; i < CATALOGUE_N; i++)
        if (matches_filter(&kCatalogue[i], query, category)) count++;
    return count;
}

int store_filtered_index(int position, const char *query, const char *category) {
    if (position < 0) return -1;
    int match = 0;
    for (int i = 0; i < CATALOGUE_N; i++) {
        if (!matches_filter(&kCatalogue[i], query, category)) continue;
        if (match++ == position) return i;
    }
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
