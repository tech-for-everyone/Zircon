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

static void about_render(struct zircapp_app *app, const zircapp_canvas_t *canvas) {
    (void)app;
    if (!canvas || !canvas->fill_rect || !canvas->draw_text)
        return;

    zircapp_battery_t battery = zircapp_battery_get();
    zircapp_network_t network = zircapp_network_get();
    char line[64];

    canvas->fill_rect(canvas->x + 12, canvas->y + 16,
                      canvas->width - 24, 84, 0xFF24243A);
    canvas->draw_text(canvas->x + 24, canvas->y + 28, 0xFF89B4FA,
                      "Zircon System Info");
    snprintf(line, sizeof(line), "Screen %dx%d",
             zircapp_phone_get_width(), zircapp_phone_get_height());
    canvas->draw_text(canvas->x + 24, canvas->y + 54, 0xFFCDD6F4, line);
    snprintf(line, sizeof(line), "PID %d  PPID %d", getpid(), getppid());
    canvas->draw_text(canvas->x + 24, canvas->y + 76, 0xFFCDD6F4, line);

    canvas->draw_text(canvas->x + 16, canvas->y + 124, 0xFFCDD6F4,
                      "Device status");
    snprintf(line, sizeof(line), "Battery %d%%%s", battery.level,
             battery.charging ? " (charging)" : "");
    canvas->draw_text(canvas->x + 16, canvas->y + 152, 0xFFA6ADC8, line);
    snprintf(line, sizeof(line), "Network signal %d  Wi-Fi %s",
             network.signal_strength, network.wifi_connected ? "up" : "down");
    canvas->draw_text(canvas->x + 16, canvas->y + 176, 0xFFA6ADC8, line);
}

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
    (void)ev;
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
    .render = about_render,
};
