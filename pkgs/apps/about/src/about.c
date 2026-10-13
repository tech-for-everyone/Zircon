/* Zircon About — system information.
 *
 * Ported from CodeOS `pkgs/core/zircon-info`, which printed PID/PPID and the
 * contents of /etc/hostname by hand.  Under ZircApp the identity still comes
 * from the native syscalls (getpid/getppid through the libc package), while
 * the device facts come from the framework's own services. */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <unistd.h>
#include <stdio.h>

/* The freestanding unistd.h declares getpid() but not getppid(); the libc
 * package (pkgs/core/base/libc) implements both over SYSCALL_GETPPID. */
extern int getppid(void);

static void about_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(),
                                    zircapp_phone_get_height(), "About");
    printf("about: Zircon System Info v1.0\n");
    printf("about: PID %d  PPID %d\n", getpid(), getppid());
    printf("about: screen %dx%d\n",
           zircapp_phone_get_width(), zircapp_phone_get_height());
}

static void about_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type != ZIRCAPP_EVENT_TOUCH_DOWN)
        return;
    {
        zircapp_battery_t b = zircapp_battery_get();
        zircapp_network_t n = zircapp_network_get();
        printf("about: battery %d%%%s  signal %d  wifi %s\n",
               b.level, b.charging ? " (charging)" : "",
               n.signal_strength, n.wifi_connected ? "up" : "down");
    }
}

static void about_stop(struct zircapp_app *app) {
    gui_window_close(app->win_id);
}

zircapp_app_t about_app = {
    .name = "About",
    .init = about_init,
    .event = about_event,
    .stop = about_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
