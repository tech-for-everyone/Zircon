/* Zircon Phone — Native C phone app */
#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include <string.h>
#include <stdio.h>

static int active_call = 0;
static char last_number[16];

static void phone_draw(int x, int y, int w, int h, uint32_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
}

static void phone_init(struct zircon_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircon_phone_get_width(), zircon_phone_get_height(), "Phone");
    printf("phone: initialized\n");
}

static void phone_event(struct zircon_app *app, const zircon_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCON_EVENT_TOUCH_DOWN) {
        int tx = ev->touch[0].x;
        int ty = ev->touch[0].y;
        int w = zircon_phone_get_width();
        int h = zircon_phone_get_height();
        /* Dial pad positions */
        if (ty < h * 0.6) {
            int num = 1 + (ty / (h * 0.2));
            if (num >= 1 && num <= 9) {
                printf("phone: dial %d\n", num);
            }
        } else if (ty < h * 0.8) {
            /* Call button */
            if (strlen(last_number) > 0) {
                zircon_phone_call(last_number);
                active_call = 1;
                printf("phone: calling %s\n", last_number);
            }
        } else {
            /* End call */
            if (active_call) {
                zircon_phone_end_call();
                active_call = 0;
                printf("phone: call ended\n");
            }
        }
    }
    if (ev->type == ZIRCON_EVENT_TOUCH_MOVE) {
        /* Update touch position for dialing */
        if (ev->touch_count > 0) {
            /* Touch move handling */
        }
    }
    if (ev->type == ZIRCON_EVENT_CALL_INCOMING) {
        printf("phone: incoming call from %s\n", ev->call.number);
        /* Show incoming call screen */
    }
    if (ev->type == ZIRCON_EVENT_SMS_RECEIVED) {
        printf("phone: SMS from %s: %s\n", ev->sms_from, ev->sms_text);
    }
}

static void phone_stop(struct zircon_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircon_app_t phone_app = {
    .name = "Phone",
    .init = phone_init,
    .event = phone_event,
    .stop = phone_stop,
    .state = ZIRCON_APP_STOPPED,
    .win_id = -1,
};

void zircon_phone_register(void) {
    zircon_app_register(&phone_app);
}
