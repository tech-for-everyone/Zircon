/* ─────────────────────────────────────────────────────────────
 * ZircApp — implementation.
 *
 * Two halves:
 *   1. the app registry + device services, moved out of the old
 *      apps/app.c and renamed to the zircapp_* API;
 *   2. zircapp_run(), the frame loop lifted out of main.c: it owns
 *      the kernel IPC poll, the input poll, event broadcast and the
 *      ~60 Hz tick, so an app only ever sees dispatcher callbacks.
 * ───────────────────────────────────────────────────────────── */
#include <zircapp.h>
#include <zircon_abi.h>

#include <string.h>
#include <stdio.h>

/* freestd unistd.h has no usleep; the kernel SLEEP syscall is in ms. */
extern int usleep(unsigned int usec);

static zircapp_app_t *apps[ZIRCAPP_MAX_APPS];
static int app_count;

void zircapp_init(void) {
    app_count = 0;
    for (int i = 0; i < ZIRCAPP_MAX_APPS; i++)
        apps[i] = 0;
}

int zircapp_register(zircapp_app_t *app) {
    if (!app || app_count >= ZIRCAPP_MAX_APPS)
        return -1;
    apps[app_count++] = app;
    app->state = ZIRCAPP_APP_RUNNING;
    if (app->init)
        app->init(app);
    return app_count - 1;
}

int zircapp_count(void) {
    return app_count;
}

zircapp_app_t *zircapp_get(int idx) {
    if (idx < 0 || idx >= app_count)
        return 0;
    return apps[idx];
}

zircapp_app_t *zircapp_find(const char *name) {
    for (int i = 0; i < app_count; i++)
        if (apps[i] && strcmp(apps[i]->name, name) == 0)
            return apps[i];
    return 0;
}

void zircapp_broadcast(const zircapp_event_t *ev) {
    for (int i = 0; i < app_count; i++) {
        if (!apps[i] || apps[i]->state == ZIRCAPP_APP_STOPPED)
            continue;
        if (apps[i]->event)
            apps[i]->event(apps[i], ev);
    }
}

void zircapp_run_all(void) {
    for (int i = 0; i < app_count; i++) {
        if (!apps[i] || apps[i]->state != ZIRCAPP_APP_RUNNING)
            continue;
        if (apps[i]->event) {
            zircapp_event_t tick = { .type = ZIRCAPP_EVENT_TICK };
            apps[i]->event(apps[i], &tick);
        }
    }
}

int zircapp_launch(const char *name) {
    zircapp_app_t *app = zircapp_find(name);
    if (!app) return -1;
    app->state = ZIRCAPP_APP_RUNNING;
    if (app->init)
        app->init(app);
    /* Find the index */
    for (int i = 0; i < app_count; i++)
        if (apps[i] == app) return i;
    return -1;
}

void zircapp_set_draw_fn(zircapp_app_t *app, zircapp_draw_fn draw) {
    if (app)
        app->draw = draw;
}

/* ── Mobile OS Implementation ── */

static zircapp_phone_state_t phone_state = ZIRCAPP_PHONE_IDLE;
static zircapp_battery_t battery = { .level = 100, .charging = 0, .plugged = 0 };
static zircapp_network_t network = { .signal_strength = 80, .network_type = 4, .wifi_connected = 1 };
static int phone_width = 360;
static int phone_height = 640;

zircapp_phone_state_t zircapp_phone_get_state(void) { return phone_state; }
void zircapp_phone_call(const char *number) {
    (void)number;
    phone_state = ZIRCAPP_PHONE_CALLING;
}
void zircapp_phone_end_call(void) { phone_state = ZIRCAPP_PHONE_IDLE; }
void zircapp_phone_answer(void) { phone_state = ZIRCAPP_PHONE_IN_CALL; }
void zircapp_phone_send_sms(const char *number, const char *text) {
    (void)number; (void)text;
}
zircapp_battery_t zircapp_battery_get(void) { return battery; }
zircapp_network_t zircapp_network_get(void) { return network; }
zircapp_sensor_data_t zircapp_sensor_read(int sensor_type) {
    (void)sensor_type;
    zircapp_sensor_data_t s = {0, 0, 0, 0};
    return s;
}
void zircapp_phone_init(void) { phone_state = ZIRCAPP_PHONE_IDLE; }
void zircapp_phone_draw_home(void) { }
void zircapp_phone_draw_call_screen(const zircapp_call_t *call) { (void)call; }
void zircapp_phone_draw_sms_screen(void) { }
void zircapp_phone_draw_app_drawer(void) { }
void zircapp_phone_draw_quick_settings(void) { }
void zircapp_touch_process(int count, const int *x, const int *y, const int *id) {
    (void)count; (void)x; (void)y; (void)id;
}
int zircapp_phone_get_width(void) { return phone_width; }
int zircapp_phone_get_height(void) { return phone_height; }
void zircapp_phone_set_screen(int width, int height) {
    phone_width = width;
    phone_height = height;
}

/* ── Frame loop ── */

/* Notification handler */
static void zircapp_handle_notify(const zircon_ipc_msg_t *msg) {
    printf("\033[1;34m[Zircon]\033[0m %s\n", msg->text);
}

int zircapp_run(zircapp_app_t *root) {
    if (zs_ipc_query() < 0)
        printf("zircon: no kernel IPC\n");
    else
        printf("zircon: IPC connected\n");

    zircapp_event_t ev;
    zircon_ipc_msg_t ipc_msg;
    zircon_input_ev_t iev;
    int prev_buttons = 0;
    int prev_mx = 0, prev_my = 0;

    while (!root || root->state != ZIRCAPP_APP_STOPPED) {
        /* Poll kernel IPC for phone events */
        while (zs_ipc_recv(&ipc_msg) == 1) {
            switch (ipc_msg.type) {
            case ZIRCON_IPC_NOTIFY:
                zircapp_handle_notify(&ipc_msg);
                break;
            case ZIRCON_IPC_APP_LAUNCH: {
                zircapp_app_t *app = zircapp_find(ipc_msg.text);
                if (app) {
                    zircapp_launch(app->name);
                    printf("launched %s\n", app->name);
                }
                break;
            }
            case ZIRCON_IPC_QS_TOGGLE:
                zircapp_phone_draw_quick_settings();
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
                    ev.type = ZIRCAPP_EVENT_TOUCH_DOWN;
                } else if (!left && was_left) {
                    ev.type = ZIRCAPP_EVENT_TOUCH_UP;
                } else if (left) {
                    ev.type = ZIRCAPP_EVENT_TOUCH_MOVE;
                } else {
                    ev.type = ZIRCAPP_EVENT_MOUSE_MOVE;
                    int tid = 0;
                    zircapp_touch_process(1, &mx, &my, &tid);
                }
                zircapp_broadcast(&ev);
            }
            prev_buttons = buttons;
            prev_mx = mx;
            prev_my = my;

            if (iev.type == 1) {
                memset(&ev, 0, sizeof(ev));
                ev.type = ZIRCAPP_EVENT_KEY_DOWN;
                ev.key = iev.key;
                zircapp_broadcast(&ev);
            }
        }

        /* Process frame tick */
        ev.type = ZIRCAPP_EVENT_TICK;
        zircapp_broadcast(&ev);

        usleep(16000); /* ~60 Hz */
    }

    printf("Zircon shutting down\n");
    return 0;
}
