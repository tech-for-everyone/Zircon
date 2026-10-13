/* Zircon SMS — Native C messaging app */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <stdio.h>
#include <string.h>

#define MAX_SMS 50
#define MAX_MSG 160

typedef struct {
    char from[16];
    char text[MAX_MSG];
    int read;
} sms_msg_t;

static sms_msg_t sms_inbox[MAX_SMS];
static int sms_count;

static void sms_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(), zircapp_phone_get_height(), "Messages");
    sms_count = 0;
    printf("sms: initialized\n");
}

static void sms_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_SMS_RECEIVED) {
        if (sms_count < MAX_SMS) {
            strncpy(sms_inbox[sms_count].from, ev->sms_from, 15);
            strncpy(sms_inbox[sms_count].text, ev->sms_text, MAX_MSG - 1);
            sms_inbox[sms_count].read = 0;
            sms_count++;
            printf("sms: new message from %s\n", ev->sms_from);
        }
    }
    if (ev->type == ZIRCAPP_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        /* Handle touch to read/compose SMS */
    }
}

static void sms_stop(struct zircapp_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircapp_app_t sms_app = {
    .name = "SMS",
    .init = sms_init,
    .event = sms_event,
    .stop = sms_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
