/* Host test for the ZircApp framework (Zircon/zircapp/zircapp.c).
 *
 * Compiles the real framework source against a shim ABI (shim/zircon_abi.h)
 * and drives it the way main.c + an app do.  No kernel, no boot: this is the
 * runtime check that a boot cannot give us, because the forked zircond has
 * no process scheduler to run it today.
 *
 * What is asserted:
 *   - register() installs the app and calls init()
 *   - find()/get()/count() index the registry
 *   - broadcast() reaches RUNNING apps and skips STOPPED ones
 *   - run_all() ticks only RUNNING apps
 *   - launch() re-runs init() and returns the index
 *   - set_draw_fn() stores the callback on the app
 *   - the device services hold their documented defaults and transitions
 *   - zircapp_run() owns the loop: it consumes IPC, dispatches APP_LAUNCH,
 *     delivers TICK, and returns when the root app is STOPPED
 */
#include "zircapp.h"
#include "zircon_abi.h"
#include <stdio.h>
#include <string.h>

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do {                       \
    checks++;                                       \
    if (cond) { printf("  ok   %s\n", msg); }       \
    else      { fails++; printf("  FAIL %s\n", msg); } \
} while (0)

/* ── test apps ── */

static int home_init_calls, home_ticks, home_broadcasts;
static int notes_init_calls, notes_broadcasts;

static void home_init(struct zircapp_app *app) { (void)app; home_init_calls++; }
static void notes_init(struct zircapp_app *app) { (void)app; notes_init_calls++; }

static void home_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    if (ev->type == ZIRCAPP_EVENT_TICK) {
        home_ticks++;
        /* End the run after 3 ticks so zircapp_run() is exercised and bounded. */
        if (home_ticks >= 3)
            app->state = ZIRCAPP_APP_STOPPED;
    }
    home_broadcasts++;
}

static void notes_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app; (void)ev;
    notes_broadcasts++;   /* every event, so "skipped" vs "reached" is observable */
}

static void dummy_draw(int x, int y, int w, int h, uint32_t c) {
    (void)x; (void)y; (void)w; (void)h; (void)c;
}

static zircapp_app_t home_app = {
    .name = "Home", .init = home_init, .event = home_event,
};
static zircapp_app_t notes_app = {
    .name = "Notes", .init = notes_init, .event = notes_event,
};

/* ── fake kernel ABI ── */

static int ipc_launch_pending = 1;

long zs_ipc_query(void) { return 0; }

int zs_ipc_recv(zircon_ipc_msg_t *msg) {
    if (ipc_launch_pending) {
        ipc_launch_pending = 0;
        memset(msg, 0, sizeof(*msg));
        msg->type = ZIRCON_IPC_APP_LAUNCH;
        strcpy(msg->text, "Notes");
        return 1;
    }
    return 0;   /* no more messages */
}

int zs_input_poll(zircon_input_ev_t *ev) { (void)ev; return -1; }

/* zircapp.c declares usleep itself; keep the frame loop from actually sleeping. */
int usleep(unsigned int usec) { (void)usec; return 0; }

/* ── main ── */

int main(void) {
    printf("zircapp host test\n");

    /* registry */
    zircapp_init();
    CHECK(zircapp_count() == 0, "fresh registry is empty");

    int idx = zircapp_register(&home_app);
    CHECK(idx == 0, "register() returns the app index (0)");
    CHECK(home_init_calls == 1, "register() calls the app init() hook");
    CHECK(home_app.state == ZIRCAPP_APP_RUNNING, "register() marks the app RUNNING");

    zircapp_register(&notes_app);
    CHECK(zircapp_count() == 2, "registry holds both apps");
    CHECK(zircapp_find("Home") == &home_app, "find() locates an app by name");
    CHECK(zircapp_find("Nope") == 0, "find() returns NULL for a missing name");
    CHECK(zircapp_get(1) == &notes_app, "get() indexes the registry");
    CHECK(zircapp_get(9) == 0, "get() rejects an out-of-range index");

    /* broadcast reaches RUNNING, skips STOPPED */
    notes_app.state = ZIRCAPP_APP_STOPPED;
    zircapp_event_t ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = ZIRCAPP_EVENT_KEY_DOWN;
    home_broadcasts = notes_broadcasts = 0;
    zircapp_broadcast(&ev);
    CHECK(home_broadcasts == 1, "broadcast() reaches a RUNNING app");
    CHECK(notes_broadcasts == 0, "broadcast() skips a STOPPED app");

    /* run_all ticks RUNNING only */
    home_ticks = notes_broadcasts = 0;
    zircapp_run_all();
    CHECK(home_ticks == 1, "run_all() ticks a RUNNING app");
    CHECK(notes_broadcasts == 0, "run_all() skips a STOPPED app");

    /* launch re-inits and returns the index */
    notes_init_calls = 0;
    int lidx = zircapp_launch("Notes");
    CHECK(lidx == 1, "launch() returns the app index");
    CHECK(notes_init_calls == 1, "launch() re-runs init()");
    CHECK(zircapp_launch("Nope") == -1, "launch() returns -1 for a missing app");

    /* draw callback is stored */
    zircapp_set_draw_fn(&home_app, dummy_draw);
    CHECK(home_app.draw == dummy_draw, "set_draw_fn() stores the callback");

    /* device services */
    zircapp_phone_init();
    CHECK(zircapp_phone_get_state() == ZIRCAPP_PHONE_IDLE, "phone starts IDLE");
    zircapp_phone_call("5551234");
    CHECK(zircapp_phone_get_state() == ZIRCAPP_PHONE_CALLING, "phone_call() -> CALLING");
    zircapp_phone_answer();
    CHECK(zircapp_phone_get_state() == ZIRCAPP_PHONE_IN_CALL, "phone_answer() -> IN_CALL");
    zircapp_phone_end_call();
    CHECK(zircapp_phone_get_state() == ZIRCAPP_PHONE_IDLE, "phone_end_call() -> IDLE");
    CHECK(zircapp_battery_get().level == 100, "battery default level is 100");
    CHECK(zircapp_network_get().wifi_connected == 1, "network default is wifi-connected");
    zircapp_phone_set_screen(800, 600);
    CHECK(zircapp_phone_get_width() == 800 && zircapp_phone_get_height() == 600,
          "set_screen()/get_width()/get_height() round-trip");

    /* frame loop: home starts RUNNING, run() must consume the queued
     * APP_LAUNCH (re-initing Notes) and stop after 3 ticks. */
    home_init_calls = notes_init_calls = home_ticks = 0;
    home_app.state = ZIRCAPP_APP_RUNNING;
    notes_app.state = ZIRCAPP_APP_STOPPED;
    ipc_launch_pending = 1;
    int rc = zircapp_run(&home_app);
    CHECK(rc == 0, "zircapp_run() returns 0 when the root app stops");
    CHECK(ipc_launch_pending == 0, "zircapp_run() drains the IPC queue");
    CHECK(notes_init_calls == 1, "zircapp_run() dispatches APP_LAUNCH to launch()");
    CHECK(home_ticks == 3, "zircapp_run() delivers TICK until the root stops");

    printf("zircapp: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
