/* Zircon Notifications — a notification centre.
 *
 * Ported from CodeOS `pkgs/core/zircon-notify`, which blocked on
 * `sys_zircon_ipc(1, &msg)` to receive kernel notifications.  Under ZircApp
 * the same messages arrive as broadcast events (SMS_RECEIVED, BATTERY_CHANGED,
 * PHONE_STATE_CHANGED, ...), so the app just records the ones it cares about. */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <string.h>
#include <stdio.h>

#define MAX_NOTES 32
#define NOTE_LEN  80

static char notes[MAX_NOTES][NOTE_LEN];
static int note_count;

static void notifications_add(const char *text) {
    if (note_count >= MAX_NOTES) {
        /* Drop the oldest so the newest is always visible. */
        memmove(notes, notes + 1, sizeof(notes) - sizeof(notes[0]));
        note_count = MAX_NOTES - 1;
    }
    strncpy(notes[note_count], text, NOTE_LEN - 1);
    notes[note_count][NOTE_LEN - 1] = '\0';
    note_count++;
    printf("notifications: %s\n", text);
}

static void notifications_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(),
                                    zircapp_phone_get_height(), "Notifications");
    notifications_add("Welcome to Zircon");
    printf("notifications: initialised (%d shown)\n", note_count);
}

static void notifications_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    switch (ev->type) {
    case ZIRCAPP_EVENT_SMS_RECEIVED:
        notifications_add("New message");
        break;
    case ZIRCAPP_EVENT_BATTERY_CHANGED:
        notifications_add("Battery changed");
        break;
    case ZIRCAPP_EVENT_PHONE_STATE_CHANGED:
        notifications_add("Phone state changed");
        break;
    case ZIRCAPP_EVENT_TOUCH_DOWN:
        /* Tapping clears the stack. */
        note_count = 0;
        printf("notifications: cleared\n");
        break;
    default:
        break;
    }
}

static void notifications_stop(struct zircapp_app *app) {
    gui_window_close(app->win_id);
}

zircapp_app_t notifications_app = {
    .name = "Notifications",
    .init = notifications_init,
    .event = notifications_event,
    .stop = notifications_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
