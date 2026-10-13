/* Zircon Notes — a small notes list.  Tap the lower half to add a note,
 * the upper half to cycle through the saved ones. */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <string.h>
#include <stdio.h>

#define MAX_NOTES 64
#define NOTE_LEN  96

static char notes[MAX_NOTES][NOTE_LEN];
static int note_count;
static int selected;

static void notes_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(),
                                    zircapp_phone_get_height(), "Notes");
    selected = 0;
    if (note_count == 0) {
        strncpy(notes[0], "Welcome to Notes", NOTE_LEN - 1);
        notes[0][NOTE_LEN - 1] = '\0';
        note_count = 1;
    }
    printf("notes: initialized (%d notes)\n", note_count);
}

static void notes_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type != ZIRCAPP_EVENT_TOUCH_DOWN || ev->touch_count <= 0)
        return;
    if (ev->touch[0].y < zircapp_phone_get_height() / 2) {
        if (note_count > 0)
            selected = (selected + 1) % note_count;
    } else if (note_count < MAX_NOTES) {
        strncpy(notes[note_count], "New note", NOTE_LEN - 1);
        notes[note_count][NOTE_LEN - 1] = '\0';
        note_count++;
    }
    printf("notes: selected %d of %d\n", selected, note_count);
}

static void notes_stop(struct zircapp_app *app) {
    gui_window_close(app->win_id);
}

zircapp_app_t notes_app = {
    .name = "Notes",
    .init = notes_init,
    .event = notes_event,
    .stop = notes_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
