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
} zircon_event_type_t;

typedef struct {
    zircon_event_type_t type;
    int mx, my;              /* mouse position */
    int key;                 /* keycode */
    int win_id;              /* target window id */
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
    int win_id;              /* -1 if no window */
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

/* ── Built-in app helpers ── */

/* Launch a native C app by name — returns its index or -1 */
int  zircon_app_launch(const char *name);

/* Set a draw callback so the app can paint into its window */
void zircon_app_set_draw_fn(zircon_app_t *app, zircon_draw_fn draw);

#endif
