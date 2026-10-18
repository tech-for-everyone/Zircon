/* Host test for the Phase-2 ZircApp *host* (pkgs/core/zircapp/src/zircapp.c).
 *
 * The Phase-1 test (../test_zircapp.c) drives the framework itself.  This
 * one drives the windowed host that Phase 2 adds: the WM command encoder,
 * the byte-stream event pump, the Home grid hit-test, and the launch/back
 * state machine.  Those all live behind the kernel fd 3/4 bridge, which is
 * why a boot cannot exercise them -- the desktop has no working mouse, and
 * the boot probe runs the host in console mode.  Here the bridge is a pair
 * of in-memory buffers.
 *
 * The real host source is included (with its main renamed) so the functions
 * under test are the shipped ones, not copies.  The real framework is
 * compiled alongside it as a separate translation unit.
 *
 * Run:  sh Zircon/zircapp/tests/host/run.sh
 * Negative control:  ZC_CONTROL=1 sh .../run.sh   (must FAIL)
 */
#define main zircapp_host_main
#include "host.c"
#undef main

#include <stdio.h>
#include <string.h>

#include "zircon_abi.h"

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do {                       \
    checks++;                                       \
    if (cond) { printf("  ok   %s\n", msg); }       \
    else      { fails++; printf("  FAIL %s\n", msg); } \
} while (0)

/* ── fake kernel bridge ── */

static uint8_t feed[256];
static int     feed_n, feed_pos;

static uint8_t cap[512];
static int     cap_n;

int sys_read(int fd, void *buf, int count) {
    (void)fd;
    int left = feed_n - feed_pos;
    if (left <= 0) return 0;
    if (count > left) count = left;
    memcpy(buf, feed + feed_pos, (size_t)count);
    feed_pos += count;
    return count;
}

int sys_pwrite(int fd, const void *buf, int count) {
    (void)fd;
    if (count > 0 && cap_n + count <= (int)sizeof(cap)) {
        memcpy(cap + cap_n, buf, (size_t)count);
        cap_n += count;
    }
    return count;
}

int sys_wm_attach(void) { return 1; }

/* The host reads the RTC (cmd 3) for the Clock screen and the kernel
 * version (cmd 0) for About; the test provides both deterministically. */
int sys_get_info(int cmd, char *buf, int max_len) {
    const char *s = "stub";
    if (cmd == 0) s = "CodeOS stub kernel";
    else if (cmd == 3) s = "2026-10-18 12:34:56";
    int len = (int)strlen(s) + 1;
    if (buf && max_len > 0) {
        if (len > max_len) len = max_len;
        memcpy(buf, s, (size_t)len);
    }
    return len;
}

/* ── fake Zircon ABI used by the framework TU ── */

long zs_ipc_query(void) { return -1; }
int  zs_ipc_recv(zircon_ipc_msg_t *msg) { (void)msg; return 0; }
int  zs_input_poll(zircon_input_ev_t *ev) { (void)ev; return -1; }
int  usleep(unsigned int usec) { (void)usec; return 0; }

/* ── test registry ── */

static int touches, key_downs;
static void probe_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_TOUCH_DOWN) touches++;
    if (ev->type == ZIRCAPP_EVENT_KEY_DOWN)   key_downs++;
}

static int phone_init_calls;
static void phone_init(struct zircapp_app *app) { (void)app; phone_init_calls++; }
static zircapp_app_t phone_app = {
    .name = "Phone", .init = phone_init, .event = probe_event,
};

/* One child is enough: the host's job is launch/back, not the fan-out. */
static int home_init_calls;
static void home_init(struct zircapp_app *app) {
    (void)app;
    home_init_calls++;
    zircapp_register(&phone_app);
}
zircapp_app_t home_app = {
    .name = "Home", .init = home_init, .event = probe_event,
};

/* ── helpers ── */

static void reset_state(void) {
    win_handle = -1; have_window = 0; mode = MODE_HOME; active_app = -1;
    feed_n = feed_pos = 0;
    cap_n = 0;
}

static void feed_conn(uint8_t h)   { feed[feed_n++] = WM_EVENT_CONN;   feed[feed_n++] = h; }
static void feed_closed(uint8_t h) { feed[feed_n++] = WM_EVENT_CLOSED; feed[feed_n++] = h; }
static void feed_key(uint8_t h, uint32_t k) {
    feed[feed_n++] = WM_EVENT_KEY; feed[feed_n++] = h;
    feed[feed_n++] = (uint8_t)(k & 0xFF);         feed[feed_n++] = (uint8_t)((k >> 8) & 0xFF);
    feed[feed_n++] = (uint8_t)((k >> 16) & 0xFF); feed[feed_n++] = (uint8_t)((k >> 24) & 0xFF);
}
static void feed_mouse(uint8_t h, int x, int y, uint8_t btn) {
    feed[feed_n++] = WM_EVENT_MOUSE; feed[feed_n++] = h;
    feed[feed_n++] = (uint8_t)(x & 0xFF);         feed[feed_n++] = (uint8_t)((x >> 8) & 0xFF);
    feed[feed_n++] = (uint8_t)(y & 0xFF);         feed[feed_n++] = (uint8_t)((y >> 8) & 0xFF);
    feed[feed_n++] = btn; feed[feed_n++] = 0;
}

/* Every byte the host has emitted since the last reset_state(). */
static int cap_is(const uint8_t *want, int n) {
    return cap_n == n && memcmp(cap, want, (size_t)n) == 0;
}

int main(void) {
    printf("zircapp host test (windowed path)\n");

    zircapp_init();
    zircapp_register(&home_app);
    CHECK(zircapp_count() == 2, "registry = Home + one child");
    CHECK(home_init_calls == 1, "registering Home ran its init once");

    /* ── WM command encodings (the bytes the kernel parses) ── */
    reset_state();
    win_handle = 5;
    wm_create(SCREEN_W, SCREEN_H, "ZircApp");
    {
        const uint8_t want[] = { WM_CREATE_WIN, 0x90,0x01, 0x58,0x02, 7,
                                 'Z','i','r','c','A','p','p' };
        CHECK(cap_is(want, (int)sizeof(want)), "wm_create encodes w,h,titlelen,title");
    }
    reset_state();
    win_handle = 5;
    wm_fill(1, 2, 3, 4, 0xAABBCCDD);
    {
        const uint8_t want[] = { WM_FILL_RECT, 5, 1,0, 2,0, 3,0, 4,0,
                                 0xDD,0xCC,0xBB,0xAA };
        CHECK(cap_is(want, (int)sizeof(want)), "wm_fill encodes rect + ARGB little-endian");
    }
    reset_state();
    win_handle = 5;
    wm_text(5, 6, 0x11223344, "Hi");
    {
        const uint8_t want[] = { WM_DRAW_STR, 5, 5,0, 6,0,
                                 0x44,0x33,0x22,0x11, 2, 'H','i' };
        CHECK(cap_is(want, (int)sizeof(want)), "wm_text encodes pos + colour + length + chars");
    }
    reset_state();
    win_handle = 5;
    wm_flush();
    { const uint8_t want[] = { WM_FLUSH, 5 }; CHECK(cap_is(want, 2), "wm_flush encodes op+handle"); }
    reset_state();
    win_handle = 5;
    wm_close();
    { const uint8_t want[] = { WM_CLOSE_WIN, 5 };
      CHECK(cap_is(want, 2), "wm_close encodes op+handle");
      CHECK(win_handle == -1, "wm_close clears the handle"); }

    /* ── event pump: CONN arms the window ── */
    reset_state();
    feed_conn(3);
    pump_events();
    CHECK(have_window == 1, "CONN arms the window");
    CHECK(win_handle == 3, "CONN records the handle");

    /* ── Home grid hit-test -> launch ── */
    reset_state();
    touches = 0;
    feed_conn(3);
    feed_mouse(3, 20, 60, 1);      /* cell 0 = Home */
    pump_events();
    CHECK(mode == MODE_APP && active_app == 0, "click on the Home cell launches index 0");
    CHECK(touches == 2, "the press is broadcast to every registered app");

    /* ── second cell -> Phone, and its init runs ── */
    phone_init_calls = 0;
    reset_state();
    feed_conn(4);
    feed_mouse(4, 210, 60, 1);     /* cell 1 = Phone */
    pump_events();
    CHECK(mode == MODE_APP && active_app == 1, "click on the Phone cell launches index 1");
    CHECK(phone_init_calls == 1, "launch runs the app init()");

    /* ── Esc goes back; a second Esc closes the window ── */
    reset_state();
    feed_conn(5);
    feed_mouse(5, 210, 60, 1);     /* launch Phone -> MODE_APP */
    feed_key(5, 27);               /* Esc -> back to the grid */
    pump_events();
    CHECK(mode == MODE_HOME && active_app == -1, "Esc from an app returns to the grid");
    CHECK(have_window == 1, "Esc from an app keeps the window open");

    reset_state();                 /* also clears the feed */
    feed_conn(5);
    feed_key(5, 27);               /* Esc on the grid -> close request */
    pump_events();
    CHECK(have_window == 0, "Esc on the grid asks to close the session");

    /* ── CLOSED tears the window down ── */
    reset_state();
    feed_conn(6);
    feed_closed(6);
    pump_events();
    CHECK(have_window == 0, "CLOSED disarms the window");
    CHECK(win_handle == 6, "the handle is left for the closer to name");

    /* ── record boundaries: CONN+MOUSE+CLOSED in one read ── */
    reset_state();
    feed_conn(7);
    feed_mouse(7, 20, 60, 1);
    feed_closed(7);
    pump_events();
    CHECK(have_window == 0, "pump consumes three records in one read");
    CHECK(mode == MODE_APP, "the middle record was dispatched, not skipped");

    /* ── a partially-arrived record must not be dispatched ── */
    reset_state();
    feed_conn(8);
    feed_mouse(8, 210, 60, 1);     /* launch Phone -> MODE_APP */
    feed[feed_n++] = WM_EVENT_KEY; feed[feed_n++] = 8;
    feed[feed_n++] = 27; feed[feed_n++] = 0;   /* only 4 of 6 bytes */
    pump_events();
    CHECK(mode == MODE_APP, "a truncated record is held back, not decoded from the tail");

    /* ── per-app screens: calculator ── */
    app_state_reset();
    calc_input('7'); calc_input('8'); calc_input('+'); calc_input('2'); calc_input('=');
    CHECK(ast.calc_acc == 80, "calculator folds 78 + 2 into 80");
    calc_input('c');
    CHECK(ast.calc_acc == 0 && ast.calc_op == 0 && !ast.calc_err,
          "C resets the calculator");
    calc_input('5'); calc_input('/'); calc_input('0'); calc_input('=');
    CHECK(ast.calc_err == 1, "divide by zero raises ERR");
    CHECK(calc_key_at(CALC_X0 + 5, CALC_Y0 + 4 * (CALC_CH + CALC_GAP) + 5) == '0',
          "bottom-left key is 0");
    CHECK(calc_key_at(CALC_X0 + 3 * (CALC_CW + CALC_GAP) + 5,
                      CALC_Y0 + 4 * (CALC_CH + CALC_GAP) + 5) == '=',
          "tall bottom-right key is =");
    CHECK(calc_key_at(CALC_X0 + 5, CALC_Y0 + 5) == 'C',
          "top-left key is C");

    /* ── clock ── */
    {
        char t[9];
        clock_hms(3661, t);
        CHECK(!strcmp(t, "01:01:01"), "clock_hms wraps 3661s to 01:01:01");
        CHECK(seg_mask(8) == 0x7F && seg_mask(1) == 0x06,
              "seven-segment masks match the classic map");
        char hms[9], date[12];
        int r = 0;
        clock_now(hms, date, &r);
        CHECK(r == 1 && !strcmp(hms, "12:34:56"),
              "clock_now reads the RTC string via GET_INFO");
        CHECK(!strcmp(date, "2026-10-18"), "clock_now splits the date out");
    }

    /* ── notes ── */
    app_state_reset();
    note_input('h'); note_input('i'); note_input('!');
    CHECK(!strcmp(ast.notes, "hi!"), "notes take typed characters");
    note_input(8);
    CHECK(!strcmp(ast.notes, "hi"), "Backspace trims the notes buffer");

    /* ── terminal ── */
    app_state_reset();
    term_input('a'); term_input('p'); term_input('p'); term_input('s');
    CHECK(!strcmp(ast.term_cur, "apps"), "terminal keeps the line being typed");
    term_input('\r');
    CHECK(ast.term_cnt == 1, "Enter posts the echoed line");
    term_input('h'); term_input('e'); term_input('l'); term_input('p'); term_input('\r');
    CHECK(ast.term_cnt == 2 && !strcmp(ast.term_lines[1], "commands: help apps time about clear"),
          "help answers with the command list");
    term_input('c'); term_input('l'); term_input('e'); term_input('a'); term_input('r'); term_input('\r');
    CHECK(ast.term_cnt == 0, "clear wipes the scrollback");

    /* ── phone: the click path through the real host routes to the pad ── */
    reset_state();
    app_state_reset();
    feed_conn(9);
    feed_mouse(9, 210, 60, 1);   /* launch Phone            */
    feed_mouse(9, 200, 240, 1);  /* dial pad: row 1, col 1  */
    pump_events();
    CHECK(!strcmp(ast.dial, "5"), "a dial pad click types the digit");
    dial_input('0'); dial_input('#');
    CHECK(!strcmp(ast.dial, "50#"), "dial accepts digits and # from the keyboard");
    CHECK(dial_call_at(DIAL_X0 + 10,
                       DIAL_Y0 + 4 * (DIAL_CH + DIAL_GAP) + 20),
          "the call button hit-box matches the drawn button");

    /* ── settings toggles ── */
    app_state_reset();
    {
        unsigned before = ast.settings;
        settings_toggle(0);
        CHECK((ast.settings & SET_WIFI) != (before & SET_WIFI),
              "tapping Wi-Fi flips its toggle");
        settings_toggle(0);
        CHECK(ast.settings == before, "tapping twice restores the toggle");
    }
    CHECK(settings_row_at(HEADER_H + 20) == 0, "row 0 hit-box starts at the top");

    /* ── music transport ── */
    app_state_reset();
    CHECK(ast.music_playing == 0, "music starts paused");
    music_toggle();
    {
        int p0 = ast.music_pos;
        music_tick();
        CHECK(ast.music_pos == p0 + 2, "TICK advances the track while playing");
        music_toggle();
        music_tick();
        CHECK(ast.music_pos == p0 + 2, "TICK is a no-op while paused");
    }

    printf("zircapp host: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
