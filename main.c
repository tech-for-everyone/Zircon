/* Zircon — the Better Android
 * Main entry point. Runs as a userspace service on CodeOS,
 * connects to kernel via IPC, manages apps and the launcher. */

#include "gui/panels.h"
#include "gui/windows.h"
#include "gui/widgets.h"
#include "apps/app.h"
#include "zircon_abi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* freestd unistd.h has no usleep; the kernel SLEEP syscall is in ms. */
extern int usleep(unsigned int usec);

/* ── Kernel IPC bridge (copy-based, via SYSCALL_ZIRCON_IPC) ── */

static int zircon_ipc_init(void) {
    if (zs_ipc_query() < 0) {
        fprintf(stderr, "zircon: no kernel IPC\n");
        return -1;
    }
    printf("zircon: IPC connected\n");
    return 0;
}

static int zircon_ipc_poll(zircon_ipc_msg_t *msg) {
    return zs_ipc_recv(msg) == 1;
}

/* ── Notification handler ── */

static void handle_notify(const zircon_ipc_msg_t *msg) {
    printf("\033[1;34m[Zircon]\033[0m %s\n", msg->text);
}

/* ── Built-in app: Home Screen ── */

/* Forward declarations for app registration */
extern zircon_app_t home_app;

/* ── Main ── */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("╔══════════════════════════════════════════╗\n");
    printf("║         Zircon OS v1.0.0 (Mobile)          ║\n");
    printf("║   Google-free mobile/desktop OS            ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    /* Connect to kernel via IPC */
    zircon_ipc_init();

    /* Set phone screen dimensions from the real framebuffer */
    zircon_fb_info_t fb;
    if (zs_fb_info(&fb) == 0 && fb.width > 0 && fb.height > 0) {
        zircon_phone_set_screen((int)fb.width, (int)fb.height);
    } else {
        zircon_phone_set_screen(360, 640);
    }
    gui_desktop_init(zircon_phone_get_width(), zircon_phone_get_height());
    gui_window_init();
    zircon_app_init();

    /* Register phone apps */
    zircon_phone_init();
    zircon_app_register(&home_app);

    printf("Zircon ready — %d app(s) registered\n", zircon_app_count());
    printf("Zircon mobile mode: touch screen %dx%d\n", zircon_phone_get_width(), zircon_phone_get_height());

    zircon_app_launch("Home");

    /* Event loop with IPC polling and touch processing */
    zircon_event_t ev;
    zircon_ipc_msg_t ipc_msg;
    zircon_input_ev_t iev;
    int prev_buttons = 0;
    int prev_mx = 0, prev_my = 0;
    int running = 1;
    while (running) {
        /* Poll kernel IPC for phone events */
        while (zircon_ipc_poll(&ipc_msg)) {
            switch (ipc_msg.type) {
            case ZIRCON_IPC_NOTIFY:
                handle_notify(&ipc_msg);
                break;
            case ZIRCON_IPC_APP_LAUNCH: {
                /* Launch an app by name */
                zircon_app_t *app = zircon_app_find(ipc_msg.text);
                if (app) {
                    zircon_app_launch(app->name);
                    printf("launched %s\n", app->name);
                }
                break;
            }
            case ZIRCON_IPC_QS_TOGGLE:
                zircon_phone_draw_quick_settings();
                break;
            default:
                break;
            }
        }

        /* Poll input once per frame: buttons → touch, motion → mouse, keys → keys. */
        if (zs_input_poll(&iev) >= 0) {
            int buttons = iev.mouse_buttons;
            int mx = iev.mouse_x;
            int my = iev.mouse_y;
            int left = (buttons & 1) != 0;
            int was_left = (prev_buttons & 1) != 0;

            if (left != was_left || mx != prev_mx || my != prev_my) {
                memset(&ev, 0, sizeof(ev));
                ev.mx = mx;
                ev.my = my;
                ev.touch_count = 1;
                ev.touch[0].id = 0;
                ev.touch[0].x = mx;
                ev.touch[0].y = my;
                ev.touch[0].pressure = 1;

                if (left && !was_left) {
                    ev.type = ZIRCON_EVENT_TOUCH_DOWN;
                } else if (!left && was_left) {
                    ev.type = ZIRCON_EVENT_TOUCH_UP;
                } else if (left) {
                    ev.type = ZIRCON_EVENT_TOUCH_MOVE;
                } else {
                    ev.type = ZIRCON_EVENT_MOUSE_MOVE;
                    int tid = 0;
                    zircon_touch_process(1, &mx, &my, &tid);
                }
                zircon_app_broadcast(&ev);
            }
            prev_buttons = buttons;
            prev_mx = mx;
            prev_my = my;

            if (iev.type == 1) {
                memset(&ev, 0, sizeof(ev));
                ev.type = ZIRCON_EVENT_KEY_DOWN;
                ev.key = iev.key;
                zircon_app_broadcast(&ev);
            }
        }

        /* Process frame tick */
        ev.type = ZIRCON_EVENT_TICK;
        zircon_app_broadcast(&ev);

        usleep(16000); /* ~60 Hz */
    }

    printf("Zircon shutting down\n");
    return 0;
}