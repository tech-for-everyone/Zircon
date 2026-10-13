/* Zircon Files — a simple file-manager list with a seeded home directory. */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <string.h>
#include <stdio.h>

#define MAX_ENTRIES 128

typedef struct {
    char name[64];
    int is_dir;
} fs_entry_t;

static fs_entry_t entries[MAX_ENTRIES];
static int entry_count;
static int selected;

static void files_seed(void) {
    static const char *names[] = {
        "Documents", "Downloads", "Pictures", "Music", "Videos", "readme.txt"
    };
    static const int dirs[] = { 1, 1, 1, 1, 1, 0 };
    unsigned i;

    entry_count = 0;
    for (i = 0; i < sizeof(names) / sizeof(names[0]) && i < MAX_ENTRIES; i++) {
        strncpy(entries[entry_count].name, names[i], sizeof(entries[entry_count].name) - 1);
        entries[entry_count].name[sizeof(entries[entry_count].name) - 1] = '\0';
        entries[entry_count].is_dir = dirs[i];
        entry_count++;
    }
}

static void files_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(),
                                    zircapp_phone_get_height(), "Files");
    files_seed();
    selected = 0;
    printf("files: initialized (%d entries)\n", entry_count);
}

static void files_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type != ZIRCAPP_EVENT_TOUCH_DOWN || ev->touch_count <= 0)
        return;
    if (entry_count > 0) {
        if (ev->touch[0].y < zircapp_phone_get_height() / 2)
            selected = (selected + 1) % entry_count;
        else
            selected = (selected - 1 + entry_count) % entry_count;
        printf("files: %s%s\n", entries[selected].name,
               entries[selected].is_dir ? "/" : "");
    }
}

static void files_stop(struct zircapp_app *app) {
    gui_window_close(app->win_id);
}

zircapp_app_t files_app = {
    .name = "Files",
    .init = files_init,
    .event = files_event,
    .stop = files_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
