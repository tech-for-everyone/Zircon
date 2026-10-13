/* Template Zircon app.
 *
 * Copy this directory to pkgs/apps/<your-app>/, rename the descriptor and
 * the file, then add the app to pkgs/apps/home/src/home.c so the launcher
 * registers it.  A package only has to provide a zircapp_app_t; the
 * framework owns the registry, the event loop and the device state.
 *
 *   cp -r pkgs/template pkgs/apps/hello
 *   mv pkgs/apps/hello/src/hello.c pkgs/apps/hello/src/hello.c
 *   # edit manifest.json (name/description/install_path) and src/hello.c
 */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <stdio.h>

static void hello_init(struct zircapp_app *app) {
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(),
                                    zircapp_phone_get_height(), "Hello");
    printf("hello: initialized\n");
}

static void hello_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_TOUCH_DOWN && ev->touch_count > 0)
        printf("hello: tap at (%d,%d)\n", ev->touch[0].x, ev->touch[0].y);
}

static void hello_stop(struct zircapp_app *app) {
    gui_window_close(app->win_id);
}

zircapp_app_t hello_app = {
    .name = "Hello",
    .init = hello_init,
    .event = hello_event,
    .stop = hello_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
