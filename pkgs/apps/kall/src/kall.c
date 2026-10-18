/* Zircon Kall — Native C dialer app */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <string.h>
#include <stdio.h>

static int active_call = 0;
static char last_number[16];

static void kall_draw(int x, int y, int w, int h, uint32_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}

static void kall_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(), zircapp_phone_get_height(), "Kall");
    printf("kall: initialized\n");
}

static void kall_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_TOUCH_DOWN) {
        int tx = ev->touch[0].x;
        int ty = ev->touch[0].y;
        int w = zircapp_phone_get_width();
        int h = zircapp_phone_get_height();
        /* Dial pad positions */
        if (ty < h * 0.6) {
            int num = 1 + (ty / (h * 0.2));
            if (num >= 1 && num <= 9) {
                printf("kall: dial %d\n", num);
            }
        } else if (ty < h * 0.8) {
            /* Call button */
            if (strlen(last_number) > 0) {
                zircapp_phone_call(last_number);
                active_call = 1;
                printf("kall: calling %s\n", last_number);
            }
        } else {
            /* End call */
            if (active_call) {
                zircapp_phone_end_call();
                active_call = 0;
                printf("kall: call ended\n");
            }
        }
    }
    if (ev->type == ZIRCAPP_EVENT_TOUCH_MOVE) {
        /* Update touch position for dialing */
        if (ev->touch_count > 0) {
            /* Touch move handling */
        }
    }
    if (ev->type == ZIRCAPP_EVENT_CALL_INCOMING) {
        printf("kall: incoming call from %s\n", ev->call.number);
        /* Show incoming call screen */
    }
    if (ev->type == ZIRCAPP_EVENT_SMS_RECEIVED) {
        printf("kall: SMS from %s: %s\n", ev->sms_from, ev->sms_text);
    }
}

static void kall_stop(struct zircapp_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircapp_app_t kall_app = {
    .name = "Kall",
    .init = kall_init,
    .event = kall_event,
    .stop = kall_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};

void zircapp_kall_register(void) {
    zircapp_register(&kall_app);
}
