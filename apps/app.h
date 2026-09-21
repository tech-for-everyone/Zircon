#ifndef ZIRCON_APP_H
#define ZIRCON_APP_H

#include <stdint.h>

#define ZIRCON_APP_NAME_MAX 48

/* Draw callback type: called by GUI toolkit to paint a rect */
typedef void (*zircon_draw_fn)(int x, int y, int w, int h, uint32_t color);

/* App event types */
typedef enum {
    ZIRCON_EVENT_NONE,
    ZIRCON_EVENT_TICK,       /* frame tick — redraw if needed */
    ZIRCON_EVENT_MOUSE_DOWN,
    ZIRCON_EVENT_MOUSE_UP,
    ZIRCON_EVENT_MOUSE_MOVE,
    ZIRCON_EVENT_KEY_DOWN,
    ZIRCON_EVENT_KEY_UP,
    ZIRCON_EVENT_FOCUS,
    ZIRCON_EVENT_BLUR,
    ZIRCON_EVENT_QUIT,
    /* Touch events for mobile */
    ZIRCON_EVENT_TOUCH_DOWN,
    ZIRCON_EVENT_TOUCH_MOVE,
    ZIRCON_EVENT_TOUCH_UP,
    ZIRCON_EVENT_TOUCH_MULTI,
    /* Phone-specific events */
    ZIRCON_EVENT_CALL_INCOMING,
    ZIRCON_EVENT_CALL_OUTGOING,
    ZIRCON_EVENT_CALL_INCOMING_RING,
    ZIRCON_EVENT_SMS_RECEIVED,
    ZIRCON_EVENT_SENSOR_ACCELEROMETER,
    ZIRCON_EVENT_SENSOR_GYROSCOPE,
    ZIRCON_EVENT_BATTERY_CHANGED,
    ZIRCON_EVENT_NETWORK_CHANGED,
    ZIRCON_EVENT_PHONE_STATE_CHANGED,
} zircon_event_type_t;

/* Touch point for multi-touch */
typedef struct {
    int id;
    int x, y;
    int pressure;
} zircon_touch_point_t;

/* Phone state */
typedef enum {
    ZIRCON_PHONE_IDLE,
    ZIRCON_PHONE_RINGING,
    ZIRCON_PHONE_CALLING,
    ZIRCON_PHONE_IN_CALL,
    ZIRCON_PHONE_OFFHOOK,
} zircon_phone_state_t;

/* Battery info */
typedef struct {
    int level;       /* 0-100 */
    int charging;    /* 0 or 1 */
    int plugged;     /* 0=none, 1=USB, 2=AC */
} zircon_battery_t;

/* Network info */
typedef struct {
    int signal_strength;  /* 0-100 */
    int network_type;     /* 0=none, 1=2G, 2=3G, 3=4G, 4=5G */
    int wifi_connected;   /* 0 or 1 */
    int wifi_ssid[33];
} zircon_network_t;

/* Sensor data */
typedef struct {
    float x, y, z;
    uint64_t timestamp;
} zircon_sensor_data_t;

/* Phone call info */
typedef struct {
    char number[16];
    char name[48];
    int state;
    int duration;
    int direction; /* 0=incoming, 1=outgoing */
} zircon_call_t;

typedef struct {
    zircon_event_type_t type;
    int mx, my;
    int key;
    int win_id;
    zircon_touch_point_t touch[5];
    int touch_count;
    zircon_call_t call;
    zircon_battery_t battery;
    zircon_network_t network;
    zircon_sensor_data_t sensor;
    char sms_from[16];
    char sms_text[160];
} zircon_event_t;

/* App state */
typedef enum {
    ZIRCON_APP_STOPPED,
    ZIRCON_APP_RUNNING,
    ZIRCON_APP_BACKGROUND,
} zircon_app_state_t;

/* Zircon app descriptor — an app fills this in and registers */
typedef struct zircon_app {
    char name[ZIRCON_APP_NAME_MAX];
    void (*init)(struct zircon_app *app);
    void (*event)(struct zircon_app *app, const zircon_event_t *ev);
    void (*stop)(struct zircon_app *app);
    zircon_app_state_t state;
    int win_id;
    void *userdata;
} zircon_app_t;

/* ── App registry API ── */

void zircon_app_init(void);
int  zircon_app_register(zircon_app_t *app);
int  zircon_app_count(void);
zircon_app_t *zircon_app_get(int idx);
zircon_app_t *zircon_app_find(const char *name);
void zircon_app_broadcast(const zircon_event_t *ev);
void zircon_app_run_all(void);

/* Launch a native C app by name — returns its index or -1 */
int  zircon_app_launch(const char *name);

/* ── Mobile OS API ── */

zircon_phone_state_t zircon_phone_get_state(void);
void zircon_phone_call(const char *number);
void zircon_phone_end_call(void);
void zircon_phone_answer(void);
void zircon_phone_send_sms(const char *number, const char *text);
zircon_battery_t zircon_battery_get(void);
zircon_network_t zircon_network_get(void);
zircon_sensor_data_t zircon_sensor_read(int sensor_type);
void zircon_phone_init(void);
void zircon_phone_draw_home(void);
void zircon_phone_draw_call_screen(const zircon_call_t *call);
void zircon_phone_draw_sms_screen(void);
void zircon_phone_draw_app_drawer(void);
void zircon_phone_draw_quick_settings(void);
void zircon_touch_process(int count, const int *x, const int *y, const int *id);
int zircon_phone_get_width(void);
int zircon_phone_get_height(void);
void zircon_phone_set_screen(int width, int height);

/* Set a draw callback so the app can paint into its window */
void zircon_app_set_draw_fn(zircon_app_t *app, zircon_draw_fn draw);

#endif
