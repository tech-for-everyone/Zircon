/* Zircon Home — Phone home screen launcher */
#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include "../apps/phone.h"
#include "../apps/sms.h"
#include "../apps/settings.h"
#include "../apps/camera.h"
#include "../apps/clock.h"
#include "../apps/calculator.h"
#include "../apps/music.h"
#include "../apps/gallery.h"
#include <string.h>
#include <stdio.h>

static int apps_registered = 0;

static void home_init(struct zircon_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircon_phone_get_width(), zircon_phone_get_height(), "Home");
    /* Register all phone apps */
    zircon_app_register(&phone_app);
    zircon_app_register(&sms_app);
    zircon_app_register(&settings_app);
    zircon_app_register(&camera_app);
    zircon_app_register(&clock_app);
    zircon_app_register(&calc_app);
    zircon_app_register(&music_app);
    zircon_app_register(&gallery_app);
    apps_registered = 1;
    printf("home: all apps registered (%d apps)\n", 8);
}

static void home_event(struct zircon_app *app, const zircon_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCON_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int tx = ev->touch[0].x;
        int ty = ev->touch[0].y;
        int w = zircon_phone_get_width();
        int h = zircon_phone_get_height();
        int col = tx / (w / 4);
        int row = ty / (h / 5);
        if (row == 0) {
            /* Top row: quick launch */
            if (col == 0) {
                /* Phone */
                printf("home: launching Phone\n");
            } else if (col == 1) {
                /* SMS */
                printf("home: launching SMS\n");
            } else if (col == 2) {
                /* Camera */
                printf("home: launching Camera\n");
            } else if (col == 3) {
                /* Music */
                printf("home: launching Music\n");
            }
        } else if (row == 1) {
            if (col == 0) {
                /* Clock */
                printf("home: launching Clock\n");
            } else if (col == 1) {
                /* Calculator */
                printf("home: launching Calculator\n");
            } else if (col == 2) {
                /* Settings */
                printf("home: launching Settings\n");
            } else if (col == 3) {
                /* Gallery */
                printf("home: launching Gallery\n");
            }
        } else if (row == 4) {
            /* Bottom row: dock */
            printf("home: dock tap at (%d,%d)\n", tx, ty);
        }
    }
    if (ev->type == ZIRCON_EVENT_TOUCH_UP && ev->touch_count > 0) {
        int h = zircon_phone_get_height();
        /* Swipe up for app drawer */
        if (ev->touch[0].y < h / 4) {
            zircon_phone_draw_app_drawer();
            printf("home: app drawer opened\n");
        }
        /* Swipe down for quick settings */
        if (ev->touch[0].y > h * 3 / 4) {
            zircon_phone_draw_quick_settings();
            printf("home: quick settings opened\n");
        }
    }
}

static void home_stop(struct zircon_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircon_app_t home_app = {
    .name = "Home",
    .init = home_init,
    .event = home_event,
    .stop = home_stop,
    .state = ZIRCON_APP_STOPPED,
    .win_id = -1,
};

void zircon_home_register(void) {
    zircon_app_register(&home_app);
}
