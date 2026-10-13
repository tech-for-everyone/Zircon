/* Zircon Home — Phone home screen launcher */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <phone.h>
#include <sms.h>
#include <settings.h>
#include <camera.h>
#include <clock.h>
#include <calculator.h>
#include <music.h>
#include <gallery.h>
#include <browser.h>
#include <notes.h>
#include <files.h>
#include <terminal.h>
#include <weather.h>
#include <about.h>
#include <notifications.h>
#include <string.h>
#include <stdio.h>

static int apps_registered = 0;

static void home_init(struct zircapp_app *app) {
    (void)app;
    /* zircapp_launch() re-runs init() on every launch, and Home is itself
     * a launchable registry entry, so re-registering the children here
     * would append a second copy of all fifteen on each tap.  Register once. */
    if (apps_registered)
        return;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(), zircapp_phone_get_height(), "Home");
    /* Register all phone apps */
    zircapp_register(&phone_app);
    zircapp_register(&sms_app);
    zircapp_register(&settings_app);
    zircapp_register(&camera_app);
    zircapp_register(&clock_app);
    zircapp_register(&calc_app);
    zircapp_register(&music_app);
    zircapp_register(&gallery_app);
    zircapp_register(&browser_app);
    zircapp_register(&notes_app);
    zircapp_register(&files_app);
    zircapp_register(&terminal_app);
    zircapp_register(&weather_app);
    zircapp_register(&about_app);
    zircapp_register(&notifications_app);
    apps_registered = 1;
    printf("home: all apps registered (%d apps)\n", 15);
}

static void home_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int tx = ev->touch[0].x;
        int ty = ev->touch[0].y;
        int w = zircapp_phone_get_width();
        int h = zircapp_phone_get_height();
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
        } else if (row == 2) {
            if (col == 0) {
                printf("home: launching Notes\n");
                zircapp_launch("Notes");
            } else if (col == 1) {
                printf("home: launching Files\n");
                zircapp_launch("Files");
            } else if (col == 2) {
                printf("home: launching Terminal\n");
                zircapp_launch("Terminal");
            } else if (col == 3) {
                printf("home: launching Weather\n");
                zircapp_launch("Weather");
            }
        } else if (row == 3) {
            if (col == 0) {
                printf("home: launching About\n");
                zircapp_launch("About");
            } else if (col == 1) {
                printf("home: launching Notifications\n");
                zircapp_launch("Notifications");
            }
        } else if (row == 4) {
            /* Bottom row: dock */
            if (col == 0) {
                /* Browser (OpenWeb) */
                printf("home: launching Browser\n");
                zircapp_launch("Browser");
            } else {
                printf("home: dock tap at (%d,%d)\n", tx, ty);
            }
        }
    }
    if (ev->type == ZIRCAPP_EVENT_TOUCH_UP && ev->touch_count > 0) {
        int h = zircapp_phone_get_height();
        /* Swipe up for app drawer */
        if (ev->touch[0].y < h / 4) {
            zircapp_phone_draw_app_drawer();
            printf("home: app drawer opened\n");
        }
        /* Swipe down for quick settings */
        if (ev->touch[0].y > h * 3 / 4) {
            zircapp_phone_draw_quick_settings();
            printf("home: quick settings opened\n");
        }
    }
}

static void home_stop(struct zircapp_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircapp_app_t home_app = {
    .name = "Home",
    .init = home_init,
    .event = home_event,
    .stop = home_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};

void zircon_home_register(void) {
    zircapp_register(&home_app);
}
