/* ─────────────────────────────────────────────────────────────
 * ZircApp — the Zircon application framework.
 *
 * ZircApp is the app-runtime layer the whole Zircon userspace is
 * built on: an app registry, an event model, the frame loop, the
 * mobile/desktop device services, and the drawing entry points.
 * It plays the same role for Zircon that `libzircon.a` plays for the
 * old toolkit, but is built around a single flat app descriptor
 * (`zircapp_app_t`) and a broadcast event bus rather than per-app
 * window plumbing.
 *
 * Porting rule: an app fills in a `zircapp_app_t`, registers it with
 * `zircapp_register()`, and paints through the draw callback.  The
 * framework owns the registry, the event loop and the device state.
 * ───────────────────────────────────────────────────────────── */
#ifndef ZIRCAPP_H
#define ZIRCAPP_H

#include <stdint.h>

#define ZIRCAPP_APP_NAME_MAX 48
#define ZIRCAPP_MAX_APPS     32

/* Draw callback type: called by the framework to paint a rect. */
typedef void (*zircapp_draw_fn)(int x, int y, int w, int h, uint32_t color);

/* ── App event types ── */
typedef enum {
    ZIRCAPP_EVENT_NONE = 0,
    ZIRCAPP_EVENT_TICK,       /* frame tick — redraw if needed */
    ZIRCAPP_EVENT_MOUSE_DOWN,
    ZIRCAPP_EVENT_MOUSE_UP,
    ZIRCAPP_EVENT_MOUSE_MOVE,
    ZIRCAPP_EVENT_KEY_DOWN,
    ZIRCAPP_EVENT_KEY_UP,
    ZIRCAPP_EVENT_FOCUS,
    ZIRCAPP_EVENT_BLUR,
    ZIRCAPP_EVENT_QUIT,
    /* Touch events for mobile */
    ZIRCAPP_EVENT_TOUCH_DOWN,
    ZIRCAPP_EVENT_TOUCH_MOVE,
    ZIRCAPP_EVENT_TOUCH_UP,
    ZIRCAPP_EVENT_TOUCH_MULTI,
    /* Phone-specific events */
    ZIRCAPP_EVENT_CALL_INCOMING,
    ZIRCAPP_EVENT_CALL_OUTGOING,
    ZIRCAPP_EVENT_CALL_INCOMING_RING,
    ZIRCAPP_EVENT_SMS_RECEIVED,
    ZIRCAPP_EVENT_SENSOR_ACCELEROMETER,
    ZIRCAPP_EVENT_SENSOR_GYROSCOPE,
    ZIRCAPP_EVENT_BATTERY_CHANGED,
    ZIRCAPP_EVENT_NETWORK_CHANGED,
    ZIRCAPP_EVENT_PHONE_STATE_CHANGED,
} zircapp_event_type_t;

/* Touch point for multi-touch */
typedef struct {
    int id;
    int x, y;
    int pressure;
} zircapp_touch_point_t;

/* Phone state */
typedef enum {
    ZIRCAPP_PHONE_IDLE,
    ZIRCAPP_PHONE_RINGING,
    ZIRCAPP_PHONE_CALLING,
    ZIRCAPP_PHONE_IN_CALL,
    ZIRCAPP_PHONE_OFFHOOK,
} zircapp_phone_state_t;

/* Battery info */
typedef struct {
    int level;       /* 0-100 */
    int charging;    /* 0 or 1 */
    int plugged;     /* 0=none, 1=USB, 2=AC */
} zircapp_battery_t;

/* Network info */
typedef struct {
    int signal_strength;  /* 0-100 */
    int network_type;     /* 0=none, 1=2G, 2=3G, 3=4G, 4=5G */
    int wifi_connected;   /* 0 or 1 */
    int wifi_ssid[33];
} zircapp_network_t;

/* Sensor data */
typedef struct {
    float x, y, z;
    uint64_t timestamp;
} zircapp_sensor_data_t;

/* Phone call info */
typedef struct {
    char number[16];
    char name[48];
    int state;
    int duration;
    int direction; /* 0=incoming, 1=outgoing */
} zircapp_call_t;

typedef struct {
    zircapp_event_type_t type;
    int mx, my;
    int key;
    int win_id;
    zircapp_touch_point_t touch[5];
    int touch_count;
    zircapp_call_t call;
    zircapp_battery_t battery;
    zircapp_network_t network;
    zircapp_sensor_data_t sensor;
    char sms_from[16];
    char sms_text[160];
} zircapp_event_t;

/* App state */
typedef enum {
    ZIRCAPP_APP_STOPPED,
    ZIRCAPP_APP_RUNNING,
    ZIRCAPP_APP_BACKGROUND,
} zircapp_app_state_t;

/* ZircApp app descriptor — an app fills this in and registers it. */
typedef struct zircapp_app {
    char name[ZIRCAPP_APP_NAME_MAX];
    void (*init)(struct zircapp_app *app);
    void (*event)(struct zircapp_app *app, const zircapp_event_t *ev);
    void (*stop)(struct zircapp_app *app);
    zircapp_app_state_t state;
    int win_id;
    void *userdata;
    zircapp_draw_fn draw;
} zircapp_app_t;

/* ── App registry API ── */

void zircapp_init(void);
int  zircapp_register(zircapp_app_t *app);
int  zircapp_count(void);
zircapp_app_t *zircapp_get(int idx);
zircapp_app_t *zircapp_find(const char *name);
void zircapp_broadcast(const zircapp_event_t *ev);
void zircapp_run_all(void);

/* Launch a native C app by name — returns its index or -1 */
int  zircapp_launch(const char *name);

/* Set a draw callback so the app can paint into its window */
void zircapp_set_draw_fn(zircapp_app_t *app, zircapp_draw_fn draw);

/* ── Main frame loop ──
 * Connects to the kernel IPC, polls input once per frame, broadcasts
 * touch/mouse/key/tick events to every registered app, and handles the
 * IPC_APP_LAUNCH / IPC_QS_TOGGLE control messages.  `root` is the app
 * that owns the session; the loop returns when it reaches
 * ZIRCAPP_APP_STOPPED.  Pass NULL to run until the process is killed. */
int zircapp_run(zircapp_app_t *root);

/* ── Mobile OS API ── */

zircapp_phone_state_t zircapp_phone_get_state(void);
void zircapp_phone_call(const char *number);
void zircapp_phone_end_call(void);
void zircapp_phone_answer(void);
void zircapp_phone_send_sms(const char *number, const char *text);
zircapp_battery_t zircapp_battery_get(void);
zircapp_network_t zircapp_network_get(void);
zircapp_sensor_data_t zircapp_sensor_read(int sensor_type);
void zircapp_phone_init(void);
void zircapp_phone_draw_home(void);
void zircapp_phone_draw_call_screen(const zircapp_call_t *call);
void zircapp_phone_draw_sms_screen(void);
void zircapp_phone_draw_app_drawer(void);
void zircapp_phone_draw_quick_settings(void);
void zircapp_touch_process(int count, const int *x, const int *y, const int *id);
int zircapp_phone_get_width(void);
int zircapp_phone_get_height(void);
void zircapp_phone_set_screen(int width, int height);

#endif /* ZIRCAPP_H */
