/* Zircon — the Better Android
 * Main entry point. Runs as a userspace service on CodeOS and Zircon.
 *
 * The shell owns the Home grid, navigation and window protocol. Built-in
 * ZircApp packages own app lifecycle; apps with a render callback paint into
 * the compositor canvas. The shell keeps a compatibility renderer for apps
 * that have not migrated yet. */

#include <panels.h>
#include <windows.h>
#include <widgets.h>
#include <zircapp.h>
#include <zircon_abi.h>
#include <wm_protocol.h>
#include <store.h>
#include <moveon.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

/* freestanding syscall wrappers (from kernel/userspace/include/unistd.h) */
static inline int sys_pwrite(int fd, const void *buf, int count) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(33), "D"(fd), "S"(buf), "d"(count) : "memory"); return ret;
}
static inline int sys_read(int fd, void *buf, int count) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(5), "D"(fd), "S"(buf), "d"(count) : "memory"); return ret;
}
static inline int sys_open(const char *path, int flags) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(4), "D"(path), "S"(flags) : "memory"); return ret;
}
static inline int sys_close(int fd) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(13), "D"(fd) : "memory"); return ret;
}
typedef struct { int size; int is_dir; } app_stat_t;
static inline int sys_stat(const char *path, app_stat_t *info) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(38), "D"(path), "S"(info) : "memory"); return ret;
}
static inline int sys_mkdir(const char *path) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(41), "D"(path) : "memory"); return ret;
}
static inline int sys_readdir(const char *path, char *names, int max_bytes) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(45), "D"(path), "S"(names), "d"(max_bytes) : "memory"); return ret;
}
static inline int sys_wm_attach(void) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_WM_ATTACH) : "memory"); return ret;
}
static inline int sys_get_info(int cmd, char *buf, int max_len) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(54), "D"(cmd), "S"(buf), "d"(max_len) : "memory"); return ret;
}

/* Settings syscalls */
#define SYSCALL_WIFI_CONTROL  81
#define SYSCALL_BT_CONTROL    82
#define SYSCALL_DND_SET       83
#define SYSCALL_DND_GET       84
#define SYSCALL_AIRPLANE_SET  85
#define SYSCALL_AIRPLANE_GET  86

#define WIFI_CMD_ENABLE     0
#define WIFI_CMD_DISABLE    1
#define WIFI_CMD_SCAN       2
#define WIFI_CMD_CONNECT    3

static inline int sys_wifi_enable(void) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_WIFI_CONTROL), "D"(WIFI_CMD_ENABLE) : "memory"); return ret;
}
static inline int sys_wifi_disable(void) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_WIFI_CONTROL), "D"(WIFI_CMD_DISABLE) : "memory"); return ret;
}
static inline int sys_wifi_status(void) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_WIFI_CONTROL), "D"(4) : "memory"); return ret;
}
static inline int sys_wifi_scan(void *out) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_WIFI_CONTROL), "D"(WIFI_CMD_SCAN), "S"(out) : "memory"); return ret;
}
static inline int sys_wifi_connect(const char *ssid, const char *password) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_WIFI_CONTROL), "D"(WIFI_CMD_CONNECT), "S"(ssid), "d"(password) : "memory"); return ret;
}

#define BT_CMD_ENABLE   0
#define BT_CMD_DISABLE  1

static inline int sys_bt_enable(void) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_BT_CONTROL), "D"(BT_CMD_ENABLE) : "memory"); return ret;
}
static inline int sys_bt_disable(void) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_BT_CONTROL), "D"(BT_CMD_DISABLE) : "memory"); return ret;
}

static inline int sys_dnd_set(int enabled) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_DND_SET), "D"(enabled) : "memory"); return ret;
}
static inline int sys_dnd_get(void) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_DND_GET) : "memory"); return ret;
}

static inline int sys_airplane_set(int enabled) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_AIRPLANE_SET), "D"(enabled) : "memory"); return ret;
}
static inline int sys_airplane_get(void) {
    int ret; __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_AIRPLANE_GET) : "memory"); return ret;
}

/* freestanding unistd.h has no usleep; provided by zircapp_platform.c */
extern int usleep(unsigned int usec);

/* ── Built-in app: Home Screen ── */
extern void zircon_apps_register(void);

/* Screen dimensions — the *logical* screen, set at runtime from the device
 * profile the kernel reports.  A 1080x2400 phone is a 360x800 logical
 * screen, and the bridge scales it up to the panel; a tablet is laid out at
 * its own width.  Must stay under the kernel bridge's 1 MiB software-canvas
 * cap: W*H*4 <= 0x100000. */
static int SCREEN_W;
static int SCREEN_H;

/* The device: its form factor, its input, and the scale the bridge should
 * apply.  Filled from the kernel's device table (SYSCALL_GET_INFO command
 * 4); a kernel without device support falls back to the framebuffer. */
static int DEVICE_SCALE = 1;
static int DEVICE_FORM  = ZIRCON_DEVICE_PHONE;
static int DEVICE_CAPS  = ZIRCON_DEVICE_TOUCH;

static const char *device_form_name(int form) {
    if (form == ZIRCON_DEVICE_TABLET) return "tablet";
    if (form == ZIRCON_DEVICE_DESKTOP) return "desktop";
    return "phone";
}

#define HEADER_H  44
#define FOOTER_H  28
#define GRID_MRG  12
#define CELL_GAP  8
#define CELL_H    57
/* Icons per row: one per ~200 logical pixels, so a phone shows two and a
 * tablet four, five or six.  It is a variable, not a constant, because the
 * device decides. */
static int COLS = 2;
static int theme_light;

/* Catppuccin-ish palette, ARGB. */
#define COL_BAR     0xFF1E1E2E
#define COL_BG      0xFF181825
#define COL_CELL    0xFF24243A
#define COL_CELL_HI 0xFF313244
#define COL_ACCENT  0xFF89B4FA
#define COL_TEXT    0xFFCDD6F4
#define COL_DIM     0xFFA6ADC8
#define COL_FAINT   0xFF6C7086
#define COL_FOOT    0xFF11111B
#define COL_OK      0xFFA6E3A1
#define COL_WARN    0xFFF9E2AF
#define COL_ERR     0xFFF38BA8
#define COL_PANEL   0xFF20203A

static uint32_t theme_color(uint32_t color) {
    if (!theme_light)
        return color;
    switch (color) {
    case COL_BAR:     return 0xFFE6E9EF;
    case COL_BG:      return 0xFFF4F5F8;
    case COL_CELL:    return 0xFFFFFFFF;
    case COL_CELL_HI: return 0xFFDCE4F2;
    case COL_TEXT:    return 0xFF252A34;
    case COL_DIM:     return 0xFF505866;
    case COL_FAINT:   return 0xFF737C8A;
    case COL_FOOT:    return 0xFFE1E4EA;
    case COL_PANEL:   return 0xFFEAECF1;
    default:          return color;
    }
}

/* App UI mode */
enum { MODE_HOME = 0, MODE_APP = 1 };
static int mode = MODE_HOME;
static int active_app_idx = -1;

/* Per-app mutable state (reset on app launch) */
struct {
    /* Calculator */
    char calc_entry[20];
    long calc_acc;
    long calc_cur;
    int  calc_op;
    int  calc_err;
    /* Kall */
    char dial[16];
    int  calling;
    /* Notes */
    char notes[384];
    int  notes_len;
    int  notes_loaded;
    int  notes_saved;
    /* Files */
    char files_path[256];
    char files_names[64][32];
    unsigned char files_dirs[64];
    int  files_count;
    int  files_scroll;
    char files_preview_name[32];
    char files_preview[512];
    int  files_preview_len;
    int  files_previewing;
    char files_notice[48];
    /* Terminal */
    char term_lines[7][44];
    int  term_cnt;
    char term_cur[44];
    int  term_len;
    /* Music */
    int  music_pos;
    int  music_playing;
    /* Settings toggles */
    unsigned settings;
    char settings_notice[48];
    /* Camera */
    int  shots;
    /* Gallery / SMS selections */
    int  tile_sel;
    int  sms_sel;
} ast;

#define SET_WIFI 0x1u
#define SET_BT   0x2u
#define SET_DARK 0x4u
#define SET_DND  0x8u
#define SET_AIR  0x10u

static unsigned saved_settings = SET_DARK;

static void app_state_reset(void) {
    memset(&ast, 0, sizeof(ast));
    ast.settings = saved_settings;
    theme_light = !(saved_settings & SET_DARK);
}

/* Forward declarations for functions used before definition */
static int hit_cell(int px, int py);
static void app_key(int key);
static void do_launch(int idx);
static int sys_read(int fd, void *buf, int count);
static int sys_wm_attach(void);

/* ── WM wire helpers (mirror kernel/kernel/user_wm.c) ── */

static int win_handle = -1;
static int have_window = 0;

static void wm_send(const void *msg, int len) { sys_pwrite(WM_PIPE_CMD, msg, len); }

static void wm_create(int w, int h, const char *title) {
    uint8_t msg[64]; int p = 0;
    msg[p++] = WM_CREATE_WIN;
    msg[p++] = (uint8_t)(w & 0xFF); msg[p++] = (uint8_t)((w >> 8) & 0xFF);
    msg[p++] = (uint8_t)(h & 0xFF); msg[p++] = (uint8_t)((h >> 8) & 0xFF);
    msg[p++] = (uint8_t)strlen(title);
    while (*title) msg[p++] = (uint8_t)*title++;
    wm_send(msg, p);
}

static uint32_t theme_color(uint32_t color);

static void wm_fill(int x, int y, int w, int h, uint32_t color) {
    if (win_handle < 0) return;
    color = theme_color(color);
    uint8_t msg[14];
    msg[0] = WM_FILL_RECT; msg[1] = (uint8_t)win_handle;
    msg[2] = (uint8_t)(x & 0xFF); msg[3] = (uint8_t)((x >> 8) & 0xFF);
    msg[4] = (uint8_t)(y & 0xFF); msg[5] = (uint8_t)((y >> 8) & 0xFF);
    msg[6] = (uint8_t)(w & 0xFF); msg[7] = (uint8_t)((w >> 8) & 0xFF);
    msg[8] = (uint8_t)(h & 0xFF); msg[9] = (uint8_t)((h >> 8) & 0xFF);
    msg[10] = (uint8_t)(color & 0xFF);
    msg[11] = (uint8_t)((color >> 8) & 0xFF);
    msg[12] = (uint8_t)((color >> 16) & 0xFF);
    msg[13] = (uint8_t)((color >> 24) & 0xFF);
    wm_send(msg, 14);
}

static void wm_text(int x, int y, uint32_t color, const char *s) {
    if (win_handle < 0 || !s) return;
    color = theme_color(color);
    int slen = (int)strlen(s); if (slen > 200) slen = 200;
    uint8_t msg[212]; int p = 0;
    msg[p++] = WM_DRAW_STR; msg[p++] = (uint8_t)win_handle;
    msg[p++] = (uint8_t)(x & 0xFF); msg[p++] = (uint8_t)((x >> 8) & 0xFF);
    msg[p++] = (uint8_t)(y & 0xFF); msg[p++] = (uint8_t)((y >> 8) & 0xFF);
    msg[p++] = (uint8_t)(color & 0xFF); msg[p++] = (uint8_t)((color >> 8) & 0xFF);
    msg[p++] = (uint8_t)((color >> 16) & 0xFF); msg[p++] = (uint8_t)((color >> 24) & 0xFF);
    msg[p++] = (uint8_t)slen;
    for (int i = 0; i < slen; i++) msg[p++] = (uint8_t)s[i];
    wm_send(msg, p);
}

static void wm_flush(void) {
    if (win_handle < 0) return;
    uint8_t msg[2] = { WM_FLUSH, (uint8_t)win_handle };
    wm_send(msg, 2);
}

static void wm_close(void) {
    if (win_handle < 0) return;
    uint8_t msg[2] = { WM_CLOSE_WIN, (uint8_t)win_handle };
    wm_send(msg, 2);
    win_handle = -1;
}

/* ── Text helpers ── */
static void text_c(int y, uint32_t col, const char *s) {
    int x = (SCREEN_W - (int)strlen(s) * 8) / 2; if (x < 0) x = 0;
    wm_text(x, y, col, s);
}
static void text_r(int xr, int y, uint32_t col, const char *s) {
    wm_text(xr - (int)strlen(s) * 8, y, col, s);
}
static const char *active_name(void) {
    zircapp_app_t *a = zircapp_get(active_app_idx);
    return a ? a->name : "";
}

/* ── Clock: seven-segment display fed by the kernel RTC ── */
static int seg_mask(int d) {
    static const int m[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    return (d >= 0 && d <= 9) ? m[d] : 0;
}
static void seg_digit(int x, int y, int w, int h, int t, int digit, uint32_t col) {
    int m = seg_mask(digit); int hh = h / 2;
    if (m & 0x01) wm_fill(x + t, y, w - 2 * t, t, col);
    if (m & 0x02) wm_fill(x + w - t, y + t, t, hh - t, col);
    if (m & 0x04) wm_fill(x + w - t, y + hh, t, hh - t, col);
    if (m & 0x08) wm_fill(x + t, y + h - t, w - 2 * t, t, col);
    if (m & 0x10) wm_fill(x, y + hh, t, hh - t, col);
    if (m & 0x20) wm_fill(x, y + t, t, hh - t, col);
    if (m & 0x40) wm_fill(x + t, y + hh - t / 2, w - 2 * t, t, col);
}
static void clock_hms(long secs, char out[9]) {
    secs %= 86400; if (secs < 0) secs += 86400;
    int h = (int)(secs / 3600); int m = (int)((secs / 60) % 60); int s = (int)(secs % 60);
    out[0] = (char)('0' + h / 10); out[1] = (char)('0' + h % 10);
    out[2] = ':'; out[3] = (char)('0' + m / 10); out[4] = (char)('0' + m % 10);
    out[5] = ':'; out[6] = (char)('0' + s / 10); out[7] = (char)('0' + s % 10); out[8] = 0;
}
static int clock_now(char hms[9], char date[12], int *is_rtc) {
    char buf[24];
    if (sys_get_info(3, buf, (int)sizeof(buf)) > 0 && buf[10] == ' ') {
        memcpy(hms, buf + 11, 8); hms[8] = 0;
        memcpy(date, buf, 10); date[10] = 0; *is_rtc = 1; return 1;
    }
    clock_hms((long)time(NULL), hms); date[0] = 0; *is_rtc = 0; return 0;
}
static void draw_clock(void) {
    char hms[9], date[12]; int is_rtc = 0; clock_now(hms, date, &is_rtc);
    int w = 30, h = 54, t = 6, gap = 8;
    int total = 6 * w + 2 * 14 + 7 * gap;
    int x = (SCREEN_W - total) / 2; int y = HEADER_H + 84;
    for (int i = 0; hms[i]; i++) {
        if (hms[i] == ':') { wm_fill(x + 4, y + h / 3, 6, 6, COL_ACCENT); wm_fill(x + 4, y + 2 * h / 3 - 6, 6, 6, COL_ACCENT); x += 14 + gap; }
        else { seg_digit(x, y, w, h, t, hms[i] - '0', COL_ACCENT); x += w + gap; }
    }
    if (is_rtc && date[0]) text_c(y + h + 22, COL_TEXT, date);
    text_c(y + h + 46, COL_FAINT, is_rtc ? "hardware clock (CMOS RTC)" : "system clock (uptime, no RTC)");
}

/* ── Calculator: integer arithmetic, left-to-right (no precedence) ── */
#define CALC_X0  12
#define CALC_Y0  128
#define CALC_CW  89
#define CALC_CH  81
#define CALC_GAP 6

static void calc_reset(void) { ast.calc_acc = 0; ast.calc_cur = 0; ast.calc_op = 0; ast.calc_err = 0; ast.calc_entry[0] = 0; }
static long calc_parse(const char *s) { long v = 0; while (*s) v = v * 10 + (*s++ - '0'); return v; }
static void calc_apply(void) {
    switch (ast.calc_op) { case 1: ast.calc_acc += ast.calc_cur; break; case 2: ast.calc_acc -= ast.calc_cur; break; case 3: ast.calc_acc *= ast.calc_cur; break; case 4: if (ast.calc_cur == 0) { ast.calc_err = 1; ast.calc_acc = 0; } else ast.calc_acc /= ast.calc_cur; break; default: ast.calc_acc = ast.calc_cur; break; }
}
static void calc_input(int ch) {
    if (ch >= '0' && ch <= '9') { if (ast.calc_err) calc_reset(); int n = (int)strlen(ast.calc_entry); if (n < (int)sizeof(ast.calc_entry) - 1) { ast.calc_entry[n] = (char)ch; ast.calc_entry[n + 1] = 0; ast.calc_cur = calc_parse(ast.calc_entry); } return; }
    if (ch == 'c' || ch == 'C') { calc_reset(); return; }
    if (ch == 'B' || ch == 8) { if (ast.calc_err) { calc_reset(); return; } int n = (int)strlen(ast.calc_entry); if (n > 0) { ast.calc_entry[n - 1] = 0; ast.calc_cur = calc_parse(ast.calc_entry); } return; }
    if (ch == '+' || ch == '-' || ch == '*' || ch == '/') { int op = ch == '+' ? 1 : ch == '-' ? 2 : ch == '*' ? 3 : 4; if (ast.calc_err) calc_reset(); if (ast.calc_op) calc_apply(); else ast.calc_acc = ast.calc_cur; if (!ast.calc_err) ast.calc_op = op; ast.calc_entry[0] = 0; ast.calc_cur = 0; return; }
    if (ch == '=' || ch == '\r' || ch == '\n') { if (ast.calc_err) { calc_reset(); return; } if (ast.calc_op) { calc_apply(); ast.calc_op = 0; } else ast.calc_acc = ast.calc_cur; ast.calc_entry[0] = 0; ast.calc_cur = ast.calc_acc; return; }
}
static char calc_key_at(int x, int y) {
    static const char g[3][4] = { { 'C', 'B', '/', '*' }, { '7', '8', '9', '-' }, { '4', '5', '6', '+' } };
    int col = (x - CALC_X0) / (CALC_CW + CALC_GAP); int row = (y - CALC_Y0) / (CALC_CH + CALC_GAP);
    if (x < CALC_X0 || y < CALC_Y0 || col > 3 || row > 4) return 0;
    if (row < 3) return g[row][col]; if (col == 3) return '='; if (row == 3) { static const char r3[3] = { '1', '2', '3' }; return r3[col]; } return '0';
}
static void calc_key_draw(int x, int y, int w, int h, char k, uint32_t bg) {
    char lbl[4]; wm_fill(x, y, w, h, bg);
    if (k == 'B') { lbl[0] = 'D'; lbl[1] = 'E'; lbl[2] = 'L'; lbl[3] = 0; } else { lbl[0] = k; lbl[1] = 0; }
    wm_text(x + (w - (int)strlen(lbl) * 8) / 2, y + (h - 16) / 2, COL_TEXT, lbl);
}
static void draw_calc(void) {
    char buf[32]; const uint32_t key_bg = COL_CELL, op_bg = 0xFF2A3A5E, clr_bg = 0xFF5E2A2A;
    wm_fill(CALC_X0, HEADER_H + 10, SCREEN_W - 2 * CALC_X0, 62, 0xFF11111B);
    if (ast.calc_err) text_r(SCREEN_W - CALC_X0 - 10, HEADER_H + 34, COL_ERR, "ERR");
    else { const char *shown = ast.calc_entry[0] ? ast.calc_entry : 0; if (shown) text_r(SCREEN_W - CALC_X0 - 10, HEADER_H + 34, COL_TEXT, shown); else { snprintf(buf, sizeof(buf), "%ld", ast.calc_acc); text_r(SCREEN_W - CALC_X0 - 10, HEADER_H + 34, COL_TEXT, buf); } }
    if (ast.calc_op) { const char *ops = "?"; switch (ast.calc_op) { case 1: ops = "+"; break; case 2: ops = "-"; break; case 3: ops = "*"; break; case 4: ops = "/"; break; } snprintf(buf, sizeof(buf), "%ld %s", ast.calc_acc, ops); wm_text(CALC_X0 + 10, HEADER_H + 20, COL_FAINT, buf); }
    for (int row = 0; row < 3; row++) for (int col = 0; col < 4; col++) {
        char k = calc_key_at(CALC_X0 + col * (CALC_CW + CALC_GAP), CALC_Y0 + row * (CALC_CH + CALC_GAP));
        uint32_t bg = (k == 'C' || k == 'B') ? clr_bg : (k >= '0' && k <= '9') ? key_bg : op_bg;
        calc_key_draw(CALC_X0 + col * (CALC_CW + CALC_GAP), CALC_Y0 + row * (CALC_CH + CALC_GAP), CALC_CW, CALC_CH, k, bg);
    }
    int ry = CALC_Y0 + 3 * (CALC_CH + CALC_GAP);
    for (int col = 0; col < 3; col++) { char k = calc_key_at(CALC_X0 + col * (CALC_CW + CALC_GAP), ry); calc_key_draw(CALC_X0 + col * (CALC_CW + CALC_GAP), ry, CALC_CW, CALC_CH, k, key_bg); }
    calc_key_draw(CALC_X0 + 3 * (CALC_CW + CALC_GAP), ry, CALC_CW, 2 * CALC_CH + CALC_GAP, '=', op_bg);
    int zy = ry + CALC_CH + CALC_GAP;
    for (int col = 0; col < 3; col++) calc_key_draw(CALC_X0 + col * (CALC_CW + CALC_GAP), zy, CALC_CW, CALC_CH, '0', key_bg);
}

/* ── Kall: dial pad + call state ── */
#define DIAL_X0  60
#define DIAL_Y0  156
#define DIAL_CW  90
#define DIAL_CH  52
#define DIAL_GAP 6

static void dial_input(int ch) { int n = (int)strlen(ast.dial); if (ch == 8 || ch == 'B') { if (n > 0) ast.dial[n - 1] = 0; return; } if ((ch >= '0' && ch <= '9') || ch == '*' || ch == '#') { if (n < 12) { ast.dial[n] = (char)ch; ast.dial[n + 1] = 0; } } }
static char dial_key_at(int x, int y) {
    static const char g[4][3] = { { '1', '2', '3' }, { '4', '5', '6' }, { '7', '8', '9' }, { '*', '0', '#' } };
    int col = (x - DIAL_X0) / (DIAL_CW + DIAL_GAP); int row = (y - DIAL_Y0) / (DIAL_CH + DIAL_GAP);
    if (x < DIAL_X0 || y < DIAL_Y0 || col > 2 || row > 3) return 0; return g[row][col];
}
static int dial_call_at(int x, int y) {
    return x >= DIAL_X0 && x < DIAL_X0 + 3 * DIAL_CW + 2 * DIAL_GAP &&
           y >= DIAL_Y0 + 4 * (DIAL_CH + DIAL_GAP) + 8 &&
           y < DIAL_Y0 + 4 * (DIAL_CH + DIAL_GAP) + 8 + 46;
}
static void draw_kall(void) {
    char buf[36];
    if (ast.calling) { snprintf(buf, sizeof(buf), "Calling %s ...", ast.dial); text_c(HEADER_H + 20, COL_OK, buf); }
    else if (ast.dial[0]) text_c(HEADER_H + 20, COL_TEXT, ast.dial);
    else text_c(HEADER_H + 20, COL_FAINT, "enter a number");
    for (int row = 0; row < 4; row++) for (int col = 0; col < 3; col++) {
        int x = DIAL_X0 + col * (DIAL_CW + DIAL_GAP); int y = DIAL_Y0 + row * (DIAL_CH + DIAL_GAP);
        wm_fill(x, y, DIAL_CW, DIAL_CH, COL_CELL);
        char k = dial_key_at(x + 2, y + 2); char lbl[2] = { k, 0 };
        wm_text(x + (DIAL_CW - 8) / 2, y + (DIAL_CH - 16) / 2, COL_TEXT, lbl);
    }
    int by = DIAL_Y0 + 4 * (DIAL_CH + DIAL_GAP) + 8;
    uint32_t bc = ast.calling ? COL_ERR : (ast.dial[0] ? COL_OK : COL_CELL_HI);
    wm_fill(DIAL_X0, by, 3 * DIAL_CW + 2 * DIAL_GAP, 46, bc);
    text_c(by + 15, 0xFF11111B, ast.calling ? "End call" : "Call");
}

/* ── Notes: one buffer, wrapped, Backspace deletes ── */
#define NOTE_COLS 44

#define NOTES_PATH "/root/root/home/Notes.txt"
#define NOTE_KEY_H 32

static void notes_load(void) {
    ast.notes_loaded = 1;
    ast.notes_len = 0;
    int fd = sys_open(NOTES_PATH, 0);
    if (fd < 0) {
        ast.notes_saved = 1;
        return;
    }
    int n = sys_read(fd, ast.notes, sizeof(ast.notes) - 1);
    int close_result = sys_close(fd);
    if (n < 0 || close_result < 0) {
        ast.notes[0] = 0;
        ast.notes_saved = 0;
        return;
    }
    ast.notes_len = n;
    ast.notes[n] = 0;
    ast.notes_saved = 1;
}

static void notes_save(void) {
    int fd = sys_open(NOTES_PATH, 1 | 0x40 | 0x200);
    if (fd < 0) {
        ast.notes_saved = 0;
        return;
    }
    int n = sys_pwrite(fd, ast.notes, ast.notes_len);
    int close_result = sys_close(fd);
    ast.notes_saved = n == ast.notes_len && close_result == 0;
}

static void note_input(int ch) {
    if (ch == 8) {
        if (ast.notes_len > 0) ast.notes[--ast.notes_len] = 0;
    } else {
        if (ch == '\r') ch = '\n';
        if ((ch == '\n' || (ch >= 32 && ch < 127)) &&
            ast.notes_len < (int)sizeof(ast.notes) - 1) {
            ast.notes[ast.notes_len++] = (char)ch;
            ast.notes[ast.notes_len] = 0;
        }
    }
    notes_save();
}

static int notes_keyboard_y(void) {
    return SCREEN_H - FOOTER_H - 4 * (NOTE_KEY_H + 4) - 8;
}

static int notes_touch_key(int x, int y) {
    int top = notes_keyboard_y();
    int row = (y - top) / (NOTE_KEY_H + 4);
    if (x < 0 || x >= SCREEN_W || y < top || row < 0 || row >= 4)
        return 0;
    static const char *rows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    if (row == 0) {
        int col = x * 10 / SCREEN_W;
        return col < 10 ? rows[0][col] : 0;
    }
    if (row == 1) {
        int width = SCREEN_W / 9;
        int start = (SCREEN_W - width * 9) / 2;
        int col = (x - start) / width;
        return x >= start && col < 9 ? rows[1][col] : 0;
    }
    if (row == 2) {
        if (x >= SCREEN_W - SCREEN_W / 5)
            return 8;
        int width = SCREEN_W / 8;
        int start = (SCREEN_W - width * 7) / 2;
        int col = (x - start) / width;
        return x >= start && col < 7 ? rows[2][col] : 0;
    }
    if (x < SCREEN_W / 5)
        return '\n';
    if (x >= SCREEN_W - SCREEN_W / 5)
        return -1;
    return ' ';
}

static void draw_note_key(int x, int y, int w, const char *label) {
    wm_fill(x + 1, y, w - 2, NOTE_KEY_H, COL_CELL);
    wm_text(x + (w - (int)strlen(label) * 8) / 2,
            y + (NOTE_KEY_H - 16) / 2, COL_TEXT, label);
}

static void draw_notes(void) {
    int x = 12, y = HEADER_H + 16; int col = 0, row = 0; char line[NOTE_COLS + 1];
    int keyboard_y = notes_keyboard_y();
    for (int i = 0; i <= ast.notes_len; i++) {
        char c = (i < ast.notes_len) ? ast.notes[i] : 0;
        if (c == '\n' || (i == ast.notes_len && col > 0)) {
            line[col] = 0;
            if (y + row * 18 < keyboard_y - 22) wm_text(x, y + row * 18, COL_TEXT, line);
            row++; col = 0; if (c == 0) break; continue;
        }
        if (i == ast.notes_len) break;
        if (col >= NOTE_COLS) {
            line[col] = 0;
            if (y + row * 18 < keyboard_y - 22) wm_text(x, y + row * 18, COL_TEXT, line);
            row++; col = 0;
        }
        line[col++] = c;
    }
    if (col == 0 && row == 0) wm_text(x, y, COL_FAINT, "Write a note. It saves as you type.");
    if (y + row * 18 < keyboard_y - 22)
        wm_fill(x + col * 8, y + row * 18, 2, 16, COL_ACCENT);
    wm_text(12, keyboard_y - 20, ast.notes_saved ? COL_OK : COL_ERR,
            ast.notes_saved ? "Saved on this device" : "Save failed - check storage");

    int ky = keyboard_y;
    int kw = SCREEN_W / 10;
    for (int i = 0; i < 10; i++) {
        char label[2] = { "qwertyuiop"[i], 0 };
        draw_note_key(i * kw, ky, kw, label);
    }
    ky += NOTE_KEY_H + 4;
    kw = SCREEN_W / 9;
    int start = (SCREEN_W - kw * 9) / 2;
    for (int i = 0; i < 9; i++) {
        char label[2] = { "asdfghjkl"[i], 0 };
        draw_note_key(start + i * kw, ky, kw, label);
    }
    ky += NOTE_KEY_H + 4;
    kw = SCREEN_W / 8;
    start = (SCREEN_W - kw * 7) / 2;
    for (int i = 0; i < 7; i++) {
        char label[2] = { "zxcvbnm"[i], 0 };
        draw_note_key(start + i * kw, ky, kw, label);
    }
    draw_note_key(SCREEN_W - SCREEN_W / 5, ky, SCREEN_W / 5, "DEL");
    ky += NOTE_KEY_H + 4;
    int side = SCREEN_W / 5;
    draw_note_key(0, ky, side, "Enter");
    draw_note_key(side, ky, SCREEN_W - side * 2, "Space");
    draw_note_key(SCREEN_W - side, ky, side, "Save");
}

/* ── Files: browse the writable user home and preview text files ── */
#define FILES_HOME "/root/root/home"
#define FILES_ROW_H 36

static int files_list_y(void) { return HEADER_H + 48; }
static int files_controls_y(void) { return SCREEN_H - FOOTER_H - 42; }

static int files_join(char *out, const char *parent, const char *name) {
    int n = snprintf(out, 256, "%s%s%s", parent,
                     parent[0] && parent[strlen(parent) - 1] == '/' ? "" : "/",
                     name);
    return n >= 0 && n < 256 ? 0 : -1;
}

static void files_refresh(void) {
    char packed[4096];
    ast.files_count = 0;
    int bytes = sys_readdir(ast.files_path, packed, sizeof(packed));
    if (bytes < 0 || bytes > (int)sizeof(packed)) {
        snprintf(ast.files_notice, sizeof(ast.files_notice), "Cannot read this folder");
        return;
    }

    for (int offset = 0; offset < bytes && ast.files_count < 64;) {
        int left = bytes - offset;
        int length = 0;
        while (length < left && packed[offset + length])
            length++;
        if (length == left) {
            snprintf(ast.files_notice, sizeof(ast.files_notice), "Invalid folder listing");
            ast.files_count = 0;
            return;
        }
        const char *name = packed + offset;
        offset += length + 1;
        if (length == 0 || (length == 1 && name[0] == '.') ||
            (length == 2 && name[0] == '.' && name[1] == '.') ||
            length >= (int)sizeof(ast.files_names[0]))
            continue;

        char path[256];
        app_stat_t info;
        if (files_join(path, ast.files_path, name) < 0 || sys_stat(path, &info) < 0)
            continue;
        int index = ast.files_count++;
        memcpy(ast.files_names[index], name, length);
        ast.files_names[index][length] = 0;
        ast.files_dirs[index] = info.is_dir != 0;
    }

    for (int i = 0; i < ast.files_count; i++) {
        for (int j = i + 1; j < ast.files_count; j++) {
            int swap = ast.files_dirs[j] > ast.files_dirs[i] ||
                (ast.files_dirs[j] == ast.files_dirs[i] &&
                 strcmp(ast.files_names[j], ast.files_names[i]) < 0);
            if (swap) {
                char name[32];
                memcpy(name, ast.files_names[i], sizeof(name));
                memcpy(ast.files_names[i], ast.files_names[j], sizeof(name));
                memcpy(ast.files_names[j], name, sizeof(name));
                unsigned char is_dir = ast.files_dirs[i];
                ast.files_dirs[i] = ast.files_dirs[j];
                ast.files_dirs[j] = is_dir;
            }
        }
    }
    if (ast.files_scroll >= ast.files_count)
        ast.files_scroll = ast.files_count > 0 ? ast.files_count - 1 : 0;
}

static void files_load_home(void) {
    snprintf(ast.files_path, sizeof(ast.files_path), "%s", FILES_HOME);
    app_stat_t info;
    if (sys_stat(ast.files_path, &info) < 0 || !info.is_dir) {
        snprintf(ast.files_notice, sizeof(ast.files_notice), "User home is unavailable");
        ast.files_count = 0;
        return;
    }
    files_refresh();
}

static void files_open_item(int index) {
    if (index < 0 || index >= ast.files_count)
        return;
    char path[256];
    if (files_join(path, ast.files_path, ast.files_names[index]) < 0)
        return;
    if (ast.files_dirs[index]) {
        snprintf(ast.files_path, sizeof(ast.files_path), "%s", path);
        ast.files_scroll = 0;
        ast.files_notice[0] = 0;
        files_refresh();
        return;
    }

    int fd = sys_open(path, 0);
    if (fd < 0) {
        snprintf(ast.files_notice, sizeof(ast.files_notice), "Cannot open this file");
        return;
    }
    int n = sys_read(fd, ast.files_preview, sizeof(ast.files_preview) - 1);
    int close_result = sys_close(fd);
    if (n < 0 || close_result < 0) {
        snprintf(ast.files_notice, sizeof(ast.files_notice), "Cannot read this file");
        return;
    }
    ast.files_preview_len = n;
    ast.files_preview[n] = 0;
    snprintf(ast.files_preview_name, sizeof(ast.files_preview_name), "%s",
             ast.files_names[index]);
    ast.files_previewing = 1;
    ast.files_notice[0] = 0;
}

static void files_go_up(void) {
    if (!strcmp(ast.files_path, FILES_HOME))
        return;
    char *slash = 0;
    for (char *p = ast.files_path; *p; p++)
        if (*p == '/') slash = p;
    if (!slash)
        return;
    *slash = 0;
    if (strlen(ast.files_path) < strlen(FILES_HOME) ||
        strncmp(ast.files_path, FILES_HOME, strlen(FILES_HOME))) {
        snprintf(ast.files_path, sizeof(ast.files_path), "%s", FILES_HOME);
    }
    ast.files_scroll = 0;
    ast.files_notice[0] = 0;
    files_refresh();
}

static void files_new_folder(void) {
    for (int i = 1; i <= 999; i++) {
        char name[24], path[256];
        snprintf(name, sizeof(name), "Folder %d", i);
        if (files_join(path, ast.files_path, name) < 0)
            break;
        app_stat_t info;
        if (sys_stat(path, &info) == 0)
            continue;
        if (sys_mkdir(path) < 0) {
            snprintf(ast.files_notice, sizeof(ast.files_notice), "Could not create folder");
            return;
        }
        snprintf(ast.files_notice, sizeof(ast.files_notice), "Created %s", name);
        files_refresh();
        return;
    }
    snprintf(ast.files_notice, sizeof(ast.files_notice), "No free folder names");
}

static void files_button(int x, int y) {
    if (y < files_controls_y())
        return;
    int button = x * 4 / SCREEN_W;
    if (ast.files_previewing) {
        if (button == 0) {
            ast.files_previewing = 0;
            ast.files_notice[0] = 0;
        }
        return;
    }
    switch (button) {
    case 0: files_go_up(); break;
    case 1: files_new_folder(); break;
    case 2:
        if (ast.files_scroll > 0) ast.files_scroll -= 8;
        files_refresh();
        break;
    case 3:
        if (ast.files_scroll + 8 < ast.files_count) ast.files_scroll += 8;
        files_refresh();
        break;
    }
}

static void files_text_line(int x, int y, const char *text, int max_chars) {
    char line[48];
    int length = (int)strlen(text);
    if (length > max_chars) length = max_chars;
    if (length >= (int)sizeof(line)) length = (int)sizeof(line) - 1;
    for (int i = 0; i < length; i++) {
        unsigned char ch = (unsigned char)text[i];
        line[i] = ch >= 32 && ch < 127 ? (char)ch : ' ';
    }
    line[length] = 0;
    wm_text(x, y, COL_TEXT, line);
}

static void draw_files(void) {
    int list_y = files_list_y();
    int controls_y = files_controls_y();
    char path_label[48];
    snprintf(path_label, sizeof(path_label), "%s", ast.files_path);
    if (strlen(path_label) > 40)
        memmove(path_label, path_label + strlen(path_label) - 40, 41);
    wm_text(12, HEADER_H + 12, COL_DIM, path_label);

    if (ast.files_previewing) {
        wm_text(12, list_y, COL_ACCENT, ast.files_preview_name);
        wm_text(12, list_y + 22, COL_FAINT, "Text preview - first 512 bytes");
        int row = 0, col = 0;
        char line[41];
        for (int i = 0; i < ast.files_preview_len && row < 12; i++) {
            unsigned char ch = (unsigned char)ast.files_preview[i];
            if (ch == '\n' || col == 40) {
                line[col] = 0;
                files_text_line(12, list_y + 48 + row * 22, line, 40);
                row++; col = 0;
                if (ch == '\n') continue;
            }
            if (ch == '\r' || ch == '\t') ch = ' ';
            line[col++] = ch >= 32 && ch < 127 ? (char)ch : '.';
        }
        if (row < 12 && col > 0) {
            line[col] = 0;
            files_text_line(12, list_y + 48 + row * 22, line, 40);
        }
    } else {
        int visible = (controls_y - list_y) / FILES_ROW_H;
        for (int row = 0; row < visible; row++) {
            int index = ast.files_scroll + row;
            if (index >= ast.files_count)
                break;
            int y = list_y + row * FILES_ROW_H;
            wm_fill(8, y, SCREEN_W - 16, FILES_ROW_H - 2, COL_CELL);
            wm_text(18, y + 9, ast.files_dirs[index] ? COL_ACCENT : COL_DIM,
                    ast.files_dirs[index] ? "[D]" : "[F]");
            wm_text(54, y + 9, COL_TEXT, ast.files_names[index]);
        }
        if (!ast.files_count)
            wm_text(12, list_y + 10, COL_FAINT, "This folder is empty");
    }

    if (ast.files_notice[0])
        wm_text(12, controls_y - 20, COL_WARN, ast.files_notice);
    int width = SCREEN_W / 4;
    static const char *labels[] = { "Up", "New folder", "Prev", "Next" };
    for (int i = 0; i < 4; i++) {
        const char *label = ast.files_previewing && i == 0 ? "Back" : labels[i];
        wm_fill(i * width + 2, controls_y, width - 4, 36, COL_CELL_HI);
        wm_text(i * width + (width - (int)strlen(label) * 8) / 2,
                controls_y + 10, COL_TEXT, label);
    }
}

static void draw_moveon(void) {
    text_c(HEADER_H + 30, COL_TEXT, "Move your files to CodeOS");
    wm_text(20, HEADER_H + 72, COL_DIM, "1. Prepare a safe copy of your home folder.");
    wm_text(20, HEADER_H + 98, COL_DIM, "2. Copy it to a computer or shared storage.");
    wm_text(20, HEADER_H + 124, COL_DIM, "3. Build and verify a .moveon bundle.");
    wm_text(20, HEADER_H + 150, COL_FAINT, "Up to 500 entries; each file <= 64 KiB.");
    wm_text(20, HEADER_H + 174, COL_FAINT, "The original files are never changed.");
    int button_y = SCREEN_H / 2 - 25;
    wm_fill(20, button_y, SCREEN_W - 40, 50, COL_ACCENT);
    text_c(button_y + 17, 0xFF11111B, "Prepare export");
    files_text_line(12, button_y + 62, moveon_status(), 40);
    wm_text(20, button_y + 94, COL_FAINT, "Host tool: Zircon/scripts/move_on.py");
}

/* ── Terminal: echo + a few builtins, 7-line scrollback ── */
static void term_push(const char *s) { if (ast.term_cnt == 7) { for (int i = 1; i < 7; i++) memcpy(ast.term_lines[i - 1], ast.term_lines[i], 44); ast.term_cnt = 6; } snprintf(ast.term_lines[ast.term_cnt], 44, "%s", s); ast.term_cnt++; }
static void term_input(int ch) {
    if (ch == 8) { if (ast.term_len > 0) ast.term_cur[--ast.term_len] = 0; return; }
    if (ch == '\r' || ch == '\n') { char line[44]; snprintf(line, sizeof(line), "%s", ast.term_cur);
        if (!strcmp(line, "help")) term_push("commands: help apps time about clear");
        else if (!strcmp(line, "apps")) { char b[40]; snprintf(b, sizeof(b), "%d apps registered", zircapp_count()); term_push(b); }
        else if (!strcmp(line, "time")) { char hms[9], date[12]; int r = 0; clock_now(hms, date, &r); char b[40]; if (r && date[0]) snprintf(b, sizeof(b), "%s %s", date, hms); else snprintf(b, sizeof(b), "%s (uptime)", hms); term_push(b); }
        else if (!strcmp(line, "about")) term_push("ZircApp host on CodeOS - type help");
        else if (!strcmp(line, "clear")) ast.term_cnt = 0;
        else if (line[0]) term_push(line);
        ast.term_len = 0; ast.term_cur[0] = 0; return; }
    if (ch >= 32 && ch < 127 && ast.term_len < 40) { ast.term_cur[ast.term_len++] = (char)ch; ast.term_cur[ast.term_len] = 0; }
}
static void draw_terminal(void) {
    int y = HEADER_H + 16; for (int i = 0; i < ast.term_cnt; i++) wm_text(12, y + i * 20, COL_DIM, ast.term_lines[i]);
    int py = y + ast.term_cnt * 20; wm_text(12, py, COL_OK, "zircapp:~$");
    char buf[48]; snprintf(buf, sizeof(buf), "%s", ast.term_cur);
    wm_text(12 + 11 * 8, py, COL_TEXT, buf);
    int cx = 12 + 11 * 8 + ast.term_len * 8; wm_fill(cx, py, 8, 16, COL_ACCENT);
}

/* ── Music: transport + progress that advances on TICK ── */
static void music_toggle(void) { ast.music_playing = !ast.music_playing; }
static void music_tick(void) { if (ast.music_playing) { ast.music_pos += 2; if (ast.music_pos > 100) ast.music_pos = 0; } }
static int music_button_at(int x, int y) { return x >= 176 && x < 224 && y >= 420 && y < 468; }
static void draw_music(void) {
    wm_fill(100, HEADER_H + 26, 200, 200, COL_PANEL);
    wm_fill(120, HEADER_H + 46, 160, 160, 0xFF2E4A7A);
    wm_fill(150, HEADER_H + 76, 100, 100, COL_ACCENT);
    text_c(HEADER_H + 256, COL_TEXT, "Zircon Theme");
    text_c(HEADER_H + 278, COL_FAINT, "The ZircApp Ensemble");
    int py = HEADER_H + 318; wm_fill(40, py, SCREEN_W - 80, 10, COL_PANEL); wm_fill(40, py, (SCREEN_W - 80) * ast.music_pos / 100, 10, COL_ACCENT);
    wm_text(40, py + 18, COL_FAINT, "0:00");
    { char t[16]; snprintf(t, sizeof(t), "1:%02d", ast.music_pos); text_r(SCREEN_W - 40, py + 18, COL_FAINT, t); }
    uint32_t bc = ast.music_playing ? COL_ACCENT : COL_CELL;
    wm_fill(176, 420, 48, 48, bc);
    if (ast.music_playing) { wm_fill(188, 434, 8, 20, 0xFF11111B); wm_fill(204, 434, 8, 20, 0xFF11111B); }
    else wm_fill(192, 434, 16, 20, 0xFF11111B);
    text_c(482, COL_FAINT, ast.music_playing ? "playing" : "paused - click to play");
}

/* ── Settings: five toggle rows ── */
static const char *kSetNames[5] = { "Wi-Fi", "Bluetooth", "Dark Mode", "Do Not Disturb", "Airplane Mode" };
static const unsigned kSetBits[5] = { SET_WIFI, SET_BT, SET_DARK, SET_DND, SET_AIR };
static int settings_row_at(int y) { int ry = HEADER_H + 14; for (int i = 0; i < 5; i++) { if (y >= ry && y < ry + 52) return i; ry += 60; } return -1; }
static void settings_toggle(int row) {
    int on;
    if (row == 0) on = sys_wifi_status() <= 0;
    else if (row == 3) on = !sys_dnd_get();
    else if (row == 4) on = !sys_airplane_get();
    else if (row == 1) {
        snprintf(ast.settings_notice, sizeof(ast.settings_notice),
                 "Bluetooth radio control is not implemented");
        return;
    } else on = !(ast.settings & kSetBits[row]);
    ast.settings &= ~kSetBits[row];
    ast.settings |= on ? kSetBits[row] : 0;
    switch (row) {
    case 0:
        if (on && sys_airplane_get()) {
            ast.settings_notice[0] = 0;
            snprintf(ast.settings_notice, sizeof(ast.settings_notice), "Turn off Airplane Mode first");
            return;
        }
        if ((on ? sys_wifi_enable() : sys_wifi_disable()) != 1) {
            ast.settings ^= SET_WIFI;
            snprintf(ast.settings_notice, sizeof(ast.settings_notice), "Wi-Fi adapter unavailable");
        } else {
            ast.settings_notice[0] = 0;
        }
        break;
    case 2:
        theme_light = !on;
        ast.settings_notice[0] = 0;
        break;
    case 3:
        if (sys_dnd_set(on) < 0) {
            ast.settings ^= SET_DND;
            snprintf(ast.settings_notice, sizeof(ast.settings_notice), "Could not change Do Not Disturb");
        } else {
            ast.settings_notice[0] = 0;
        }
        break;
    case 4:
        if (sys_airplane_set(on) < 0) {
            ast.settings ^= SET_AIR;
            snprintf(ast.settings_notice, sizeof(ast.settings_notice), "Could not change Airplane Mode");
        } else {
            if (on) {
                sys_wifi_disable();
                ast.settings &= ~SET_WIFI;
            }
            ast.settings_notice[0] = 0;
        }
        break;
    }
    saved_settings = ast.settings;
}
static void draw_settings(void) {
    int ry = HEADER_H + 14; for (int i = 0; i < 5; i++) {
        int on = (ast.settings & kSetBits[i]) != 0;
        if (i == 0) on = sys_wifi_status() > 0;
        else if (i == 1) on = sys_bt_enable() > 0;
        if (i == 3) on = sys_dnd_get();
        else if (i == 4) on = sys_airplane_get();
        wm_fill(12, ry, SCREEN_W - 24, 52, COL_CELL); wm_text(28, ry + 18, COL_TEXT, kSetNames[i]);
        if (i == 1) {
            const char *status = on ? "detected" : "not available";
            wm_text(SCREEN_W - 28 - (int)strlen(status) * 8, ry + 18, COL_FAINT, status);
        } else {
            int px = SCREEN_W - 12 - 28 - 44;
            wm_fill(px, ry + 14, 44, 24, on ? COL_ACCENT : COL_PANEL);
            wm_fill(on ? px + 22 : px + 2, ry + 16, 20, 20, 0xFF11111B);
        }
        ry += 60;
    }
    if (ast.settings_notice[0])
        wm_text(16, ry + 8, COL_WARN, ast.settings_notice);
}

/* ── Gallery: 2x3 tiles, click selects ── */
static int gallery_tile_at(int x, int y) {
    int gx = 12, gy = HEADER_H + 16, tw = 120, th = 150;
    for (int i = 0; i < 6; i++) { int tx = gx + (i % 3) * (tw + 8); int ty = gy + (i / 3) * (th + 8); if (x >= tx && x < tx + tw && y >= ty && y < ty + th) return i; } return -1; }
static void draw_gallery(void) {
    static const uint32_t cols[6] = { 0xFFE78284, 0xFFA6E3A1, 0xFF89B4FA, 0xFFF9E2AF, 0xFFCBA6F7, 0xFF94E2D5 };
    static const char *names[6] = { "Sunset", "Forest", "Ocean", "Sand", "Violet", "Mint" };
    for (int i = 0; i < 6; i++) {
        int tx = 12 + (i % 3) * 128; int ty = HEADER_H + 16 + (i / 3) * 158;
        wm_fill(tx, ty, 120, 120, cols[i]);
        if (i == ast.tile_sel) { wm_fill(tx - 3, ty - 3, 126, 3, COL_TEXT); wm_fill(tx - 3, ty + 120, 126, 3, COL_TEXT); wm_fill(tx - 3, ty, 3, 120, COL_TEXT); wm_fill(tx + 120, ty, 3, 120, COL_TEXT); }
        wm_text(tx + (120 - (int)strlen(names[i]) * 8) / 2, ty + 124, i == ast.tile_sel ? COL_TEXT : COL_DIM, names[i]);
    }
}

/* ── SMS: thread list, click selects ── */
static const char *kSmsFrom[3] = {"ZircApp", "Network" };
static const char *kSmsText[3] = {"welcome to Zircon!", "your data is safe" };
static int sms_row_at(int y) { int ry = HEADER_H + 14; for (int i = 0; i < 3; i++) { if (y >= ry && y < ry + 56) return i; ry += 64; } return -1; }
static void draw_sms(void) {
    int ry = HEADER_H + 14; for (int i = 0; i < 3; i++) {
        int sel = (i == ast.sms_sel);
        wm_fill(12, ry, SCREEN_W - 24, 56, sel ? COL_CELL_HI : COL_CELL);
        wm_fill(12, ry, 4, 56, sel ? COL_ACCENT : COL_PANEL);
        wm_text(28, ry + 10, COL_TEXT, kSmsFrom[i]); wm_text(28, ry + 30, COL_FAINT, kSmsText[i]); ry += 64;
    }
    text_c(SCREEN_H - FOOTER_H - 26, COL_FAINT, ast.sms_sel >= 0 ? "thread selected" : "click a thread");
}

/* ── Weather: one card + a four-day strip ── */
static void draw_weather(void) {
    wm_fill(12, HEADER_H + 16, SCREEN_W - 24, 150, COL_PANEL);
    text_c(HEADER_H + 34, COL_TEXT, "Zircon City"); text_c(HEADER_H + 62, COL_ACCENT, "21 C");
    text_c(HEADER_H + 96, COL_DIM, "partly cloudy"); text_c(HEADER_H + 118, COL_FAINT, "wind 12 km/h - humidity 48%");
    static const char *days[4] = { "Mon", "Tue", "Wed", "Thu" };
    static const char *tems[4] = { "22 C", "19 C", "24 C", "18 C" };
    for (int i = 0; i < 4; i++) { int cx = 12 + i * 95; wm_fill(cx, HEADER_H + 186, 88, 96, COL_CELL); wm_text(cx + (88 - 3 * 8) / 2, HEADER_H + 200, COL_DIM, days[i]); wm_text(cx + (88 - 4 * 8) / 2, HEADER_H + 224, COL_TEXT, tems[i]); wm_fill(cx + 20, HEADER_H + 250, 48, 6, i == 0 ? COL_ACCENT : COL_PANEL); }
}

/* ── Browser: URL bar + an OpenWeb card ── */
static void draw_browser_app(void) {
    wm_fill(12, HEADER_H + 14, SCREEN_W - 104, 36, COL_PANEL); wm_text(24, HEADER_H + 24, COL_TEXT, "about:zircon");
    wm_fill(SCREEN_W - 82, HEADER_H + 14, 70, 36, COL_ACCENT); text_c(HEADER_H + 24, 0xFF11111B, "Go");
    wm_fill(12, HEADER_H + 64, SCREEN_W - 24, 190, COL_CELL);
    wm_text(28, HEADER_H + 82, COL_ACCENT, "OpenWeb engine");
    wm_text(28, HEADER_H + 108, COL_DIM, "Rust HTTP + HTML renderer,");
    wm_text(28, HEADER_H + 128, COL_DIM, "no WebKit, no TLS stack.");
    wm_text(28, HEADER_H + 158, COL_FAINT, "The windowed browser is the");
    wm_text(28, HEADER_H + 178, COL_FAINT, "openweb app on the desktop.");
    wm_text(28, HEADER_H + 210, COL_TEXT, "try: http://10.0.2.2:8095/");
}

/* ── Camera: viewfinder, clicking snaps a shot ── */
static void draw_camera(void) {
    wm_fill(8, HEADER_H + 14, SCREEN_W - 16, 380, 0xFF0C0C14);
    uint32_t c = COL_ACCENT;
    wm_fill(20, HEADER_H + 26, 36, 4, c); wm_fill(20, HEADER_H + 26, 4, 36, c);
    wm_fill(SCREEN_W - 56, HEADER_H + 26, 36, 4, c); wm_fill(SCREEN_W - 24, HEADER_H + 26, 4, 36, c);
    wm_fill(20, HEADER_H + 368, 36, 4, c); wm_fill(20, HEADER_H + 336, 4, 36, c);
    wm_fill(SCREEN_W - 56, HEADER_H + 368, 36, 4, c); wm_fill(SCREEN_W - 24, HEADER_H + 336, 4, 36, c);
    if (ast.shots > 0) { char b[32]; snprintf(b, sizeof(b), "captured %d photo%s", ast.shots, ast.shots == 1 ? "" : "s"); text_c(HEADER_H + 210, COL_OK, b); }
    else text_c(HEADER_H + 210, COL_FAINT, "click to snap a photo");
    wm_fill(184, HEADER_H + 414, 32, 32, ast.shots ? COL_ERR : COL_CELL_HI);
}

/* ── About: identity + kernel info ── */
static void draw_about(void) {
    char kver[64]; if (sys_get_info(0, kver, (int)sizeof(kver)) <= 0) snprintf(kver, sizeof(kver), "CodeOS");
    wm_fill(172, HEADER_H + 24, 56, 56, COL_ACCENT);
    text_c(HEADER_H + 92, COL_TEXT, "Zircon"); text_c(HEADER_H + 114, COL_FAINT, "the Better Android");
    text_c(HEADER_H + 152, COL_DIM, "native C apps - no VM - no Google");
    text_c(HEADER_H + 182, COL_DIM, "ZircApp framework on the CodeOS kernel");
    text_c(HEADER_H + 216, COL_FAINT, kver);
    { char b[48]; snprintf(b, sizeof(b), "%d apps registered", zircapp_count()); text_c(HEADER_H + 246, COL_FAINT, b); }
    text_c(HEADER_H + 286, COL_FAINT, "v1.0.0");
}

/* ── Zircon Store: the app catalogue ── */
#define STORE_ROW_H   44
#define STORE_X       16
#define STORE_TOP     (HEADER_H + 106)
#define STORE_BADGE_W 108

static int store_scroll;   /* first catalogue row on screen */
static int store_category_sel;
static int store_detail_idx = -1;
static int store_search_active;
static char store_search[32];

static int store_keyboard_y(void) {
    return SCREEN_H - FOOTER_H - 126;
}

static void store_search_input(int key) {
    size_t length = strlen(store_search);
    if (key == 8 || key == 127) {
        if (length) store_search[length - 1] = 0;
    } else if (key == 13) {
        store_search_active = 0;
    } else if (key >= 32 && key < 127 && length + 1 < sizeof(store_search)) {
        store_search[length] = (char)key;
        store_search[length + 1] = 0;
    }
    store_scroll = 0;
}

static void draw_store_keyboard(void) {
    static const char *const rows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    const int row_y = store_keyboard_y();
    const int gap = 3;
    for (int row = 0; row < 3; row++) {
        int count = (int)strlen(rows[row]);
        int key_w = (SCREEN_W - STORE_X * 2 - gap * (count - 1)) / count;
        int row_w = count * key_w + (count - 1) * gap;
        int left = (SCREEN_W - row_w) / 2;
        int y = row_y + row * 32;
        for (int col = 0; col < count; col++) {
            int x = left + col * (key_w + gap);
            char label[2] = { rows[row][col], 0 };
            wm_fill(x, y, key_w, 28, COL_CELL);
            wm_text(x + (key_w - 8) / 2, y + 7, COL_TEXT, label);
        }
    }
    int action_y = row_y + 96;
    int action_w = (SCREEN_W - STORE_X * 2 - 2 * gap) / 3;
    static const char *const actions[] = { "Space", "Backspace", "Done" };
    for (int i = 0; i < 3; i++) {
        int x = STORE_X + i * (action_w + gap);
        wm_fill(x, action_y, action_w, 28, COL_PANEL);
        wm_text(x + 6, action_y + 7, i == 2 ? COL_OK : COL_TEXT, actions[i]);
    }
}

static void store_search_key_at(int x, int y) {
    static const char *const rows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    int relative_y = y - store_keyboard_y();
    int row = relative_y / 32;
    if (relative_y < 0 || relative_y >= 124) return;
    if (row < 3) {
        int count = (int)strlen(rows[row]);
        int gap = 3;
        int key_w = (SCREEN_W - STORE_X * 2 - gap * (count - 1)) / count;
        int row_w = count * key_w + (count - 1) * gap;
        int left = (SCREEN_W - row_w) / 2;
        int col = (x - left) / (key_w + gap);
        if (x >= left && col >= 0 && col < count &&
            x < left + row_w && (x - left) % (key_w + gap) < key_w)
            store_search_input(rows[row][col]);
        return;
    }
    int gap = 3;
    int action_w = (SCREEN_W - STORE_X * 2 - 2 * gap) / 3;
    int relative_x = x - STORE_X;
    int action = relative_x / (action_w + gap);
    if (relative_x < 0 || action < 0 || action >= 3 ||
        relative_x % (action_w + gap) >= action_w)
        return;
    if (action == 0) store_search_input(' ');
    else if (action == 1) store_search_input(8);
    else store_search_input(13);
}

static const char *store_category_name(void) {
    const char *category = store_category(store_category_sel);
    return category ? category : "All";
}

static int store_rows_visible(void) {
    int reserved = store_search_active ? 126 : 34;
    int space = SCREEN_H - FOOTER_H - STORE_TOP - reserved;
    if (space < STORE_ROW_H) return 1;
    return space / STORE_ROW_H;
}

/* Text is 8 px per cell in the WM font, so a description is clipped by
 * counting cells rather than by measuring: wm_text has no clipping and a
 * long string would run into the badge. */
static void store_text_clipped(int x, int y, uint32_t col, const char *s, int max_cells) {
    char buf[128];
    int n = 0;
    if (!s) return;
    while (s[n] && n < max_cells && n < (int)sizeof(buf) - 1) { buf[n] = s[n]; n++; }
    buf[n] = 0;
    wm_text(x, y, col, buf);
}

static void draw_store(void) {
    int total = store_filtered_count(store_search, store_category_name());
    int vis = store_rows_visible();
    if (store_scroll > total - vis) store_scroll = total - vis;
    if (store_scroll < 0) store_scroll = 0;

    char buf[96];
    if (store_detail_idx >= 0) {
        const store_item_t *it = store_item(store_detail_idx);
        if (!it) {
            store_detail_idx = -1;
            return;
        }
        int installed = store_is_installed(it->name);
        wm_text(STORE_X, HEADER_H + 20, COL_TEXT, it->name);
        snprintf(buf, sizeof(buf), "v%s  %s  %u KB", it->version, it->category, it->size_kb);
        wm_text(STORE_X, HEADER_H + 46, COL_FAINT, buf);
        store_text_clipped(STORE_X, HEADER_H + 88, COL_DIM, it->desc,
                           (SCREEN_W - STORE_X * 2) / 8);
        wm_text(STORE_X, HEADER_H + 132, COL_FAINT, "Requested capabilities:");
        store_text_clipped(STORE_X, HEADER_H + 152, COL_ACCENT, it->permissions, 48);
        wm_text(STORE_X, HEADER_H + 172, COL_FAINT, "Disclosure only; runtime grants are not enforced yet.");
        wm_text(STORE_X, HEADER_H + 194, COL_FAINT,
                installed ? "Available in the Home app grid." : "Built into this Zircon image.");
        int button_y = SCREEN_H - FOOTER_H - 52;
        int half = SCREEN_W / 2;
        wm_fill(STORE_X, button_y, half - STORE_X - 4, 38, COL_PANEL);
        wm_text(STORE_X + 10, button_y + 12, COL_OK, installed ? "Open app" : "Add to Home");
        wm_fill(half + 4, button_y, SCREEN_W - half - STORE_X - 4, 38, COL_PANEL);
        wm_text(half + 14, button_y + 12, installed ? COL_ACCENT : COL_DIM,
                installed ? "Remove" : "Back");
        return;
    }

    snprintf(buf, sizeof(buf), "%d matching apps", total);
    wm_text(STORE_X, HEADER_H + 6, COL_DIM, buf);
    int search_y = HEADER_H + 24;
    wm_fill(STORE_X, search_y, SCREEN_W - STORE_X * 2, 26,
            store_search_active ? COL_CELL_HI : COL_CELL);
    snprintf(buf, sizeof(buf), "Search: %s%s", store_search[0] ? store_search : "tap and type",
             store_search_active ? "_" : "");
    store_text_clipped(STORE_X + 8, search_y + 7, COL_TEXT, buf,
                       (SCREEN_W - STORE_X * 2 - 16) / 8);
    int category_y = HEADER_H + 58;
    const char *category = store_category_name();
    snprintf(buf, sizeof(buf), "Category: %s  (tap to change)", category);
    store_text_clipped(STORE_X, category_y, COL_ACCENT, buf,
                       (SCREEN_W - STORE_X * 2) / 8);
    wm_text(SCREEN_W - 128, category_y, COL_FAINT, "Enter to search");

    for (int r = 0; r < vis; r++) {
        int idx = store_filtered_index(store_scroll + r, store_search, category);
        const store_item_t *it = store_item(idx);
        if (!it) break;
        int y = STORE_TOP + r * STORE_ROW_H;
        int installed = store_is_installed(it->name);
        wm_fill(STORE_X, y, SCREEN_W - STORE_X * 2, STORE_ROW_H - 6, COL_CELL);
        wm_fill(STORE_X, y, 3, STORE_ROW_H - 6, installed ? COL_OK : COL_ACCENT);
        wm_text(STORE_X + 12, y + 5, COL_TEXT, it->name);
        snprintf(buf, sizeof(buf), "v%s  %s  %u KB", it->version, it->category, it->size_kb);
        wm_text(STORE_X + 12, y + 24, COL_FAINT, buf);
        store_text_clipped(STORE_X + 190, y + 24, COL_DIM, it->desc,
                           (SCREEN_W - STORE_X * 2 - STORE_BADGE_W - 210) / 8);
        /* The row opens details; Home actions are shown there. */
        const char *badge = installed ? "ON HOME" : "DETAILS";
        uint32_t badge_col = installed ? COL_OK : COL_ACCENT;
        int badge_x = SCREEN_W - STORE_X - STORE_BADGE_W;
        wm_fill(badge_x, y, STORE_BADGE_W - 8, STORE_ROW_H - 6, COL_PANEL);
        wm_text(badge_x + 12, y + 14, badge_col, badge);
    }

    if (total > vis) {
        snprintf(buf, sizeof(buf), "%d of %d - tap below to scroll", store_scroll + vis, total);
        wm_text(STORE_X, SCREEN_H - FOOTER_H - 22, COL_FAINT, buf);
    } else if (total == 0) {
        wm_text(STORE_X, STORE_TOP + 12, COL_FAINT, "No apps match this search.");
    }
    if (store_search_active)
        draw_store_keyboard();
}

/* Which catalogue row is at this y?  -1 when the tap is past the list. */
static int store_row_at(int y) {
    if (y < STORE_TOP) return -1;
    int r = (y - STORE_TOP) / STORE_ROW_H;
    int vis = store_rows_visible();
    if (r >= vis) return -1;
    return store_filtered_index(store_scroll + r, store_search, store_category_name());
}

/* The badge is the right-hand strip of the row. */
static void store_tap(int x, int y) {
    if (store_detail_idx >= 0) {
        int button_y = SCREEN_H - FOOTER_H - 52;
        if (y < button_y || y >= button_y + 38) return;
        const store_item_t *selected = store_item(store_detail_idx);
        if (!selected) {
            store_detail_idx = -1;
            return;
        }
        if (x < SCREEN_W / 2) {
            if (!store_is_installed(selected->name)) {
                store_set_installed(selected->name, 1);
                printf("store: '%s' added to Home\n", selected->name);
                return;
            }
            for (int i = 0; i < zircapp_count(); i++) {
                zircapp_app_t *app = zircapp_get(i);
                if (app && !strcmp(app->name, selected->name)) {
                    do_launch(i);
                    return;
                }
            }
            printf("store: '%s' is not in the app registry\n", selected->name);
            return;
        }
        if (store_is_installed(selected->name)) {
            store_set_installed(selected->name, 0);
            printf("store: '%s' removed from Home\n", selected->name);
        } else {
            store_detail_idx = -1;
        }
        return;
    }

    if (store_search_active && y >= store_keyboard_y()) {
        store_search_key_at(x, y);
        return;
    }
    if (y >= HEADER_H + 24 && y < HEADER_H + 52) {
        store_search_active = 1;
        return;
    }
    if (y >= HEADER_H + 52 && y < STORE_TOP) {
        store_category_sel++;
        if (store_category_sel >= store_category_count()) store_category_sel = 0;
        store_scroll = 0;
        return;
    }
    int idx = store_row_at(y);
    if (idx < 0) {
        /* A tap past the last row scrolls the list, if there is more. */
        int vis = store_rows_visible();
        int total = store_filtered_count(store_search, store_category_name());
        if (store_scroll + vis < total) {
            store_scroll += vis;
            printf("store: scrolled to %d\n", store_scroll);
        }
        return;
    }
    const store_item_t *it = store_item(idx);
    if (!it) return;
    (void)x;
    store_detail_idx = idx;
    store_search_active = 0;
}

/* ── Layout + painting ── */
static void cell_rect(int idx, int *x, int *y, int *w, int *h) {
    int cols = COLS > 0 ? COLS : 2;
    int cw = (SCREEN_W - GRID_MRG * (cols + 1)) / cols;
    *x = GRID_MRG + (idx % cols) * (cw + GRID_MRG);
    *y = HEADER_H + GRID_MRG + (idx / cols) * (CELL_H + CELL_GAP);
    *w = cw; *h = CELL_H;
}

/* The Home grid shows the INSTALLED apps only -- the Zircon Store owns that
 * set (pkgs/apps/store).  Drawing and hit-testing therefore have to walk the
 * same list, or a tap lands on a different app than the one that was drawn.
 * home_registry_at() is the single mapping both use. */
static int home_registry_at(int disp) {
    int seen = 0;
    for (int i = 0; i < zircapp_count(); i++) {
        zircapp_app_t *a = zircapp_get(i);
        if (!a || !store_is_installed(a->name)) continue;
        if (seen == disp) return i;
        seen++;
    }
    return -1;
}

static void draw_status(void) {
    zircapp_battery_t b = zircapp_battery_get(); zircapp_network_t n = zircapp_network_get();
    char buf[80]; snprintf(buf, sizeof(buf), "battery %d%%  net %d  wifi %d", b.level, n.network_type, n.wifi_connected);
    wm_fill(0, SCREEN_H - FOOTER_H, SCREEN_W, FOOTER_H, COL_FOOT);
    wm_text(8, SCREEN_H - FOOTER_H + (FOOTER_H - 16) / 2, COL_DIM, buf);
}
static void draw_home(void) {
    char buf[48];
    int shown = 0;
    for (int i = 0; i < zircapp_count(); i++) {
        zircapp_app_t *a = zircapp_get(i);
        if (a && store_is_installed(a->name)) shown++;
    }
    wm_fill(0, 0, SCREEN_W, HEADER_H, COL_BAR);
    wm_text(12, (HEADER_H - 16) / 2, COL_TEXT, "Zircon");
    snprintf(buf, sizeof(buf), "%d apps", shown);
    wm_text(SCREEN_W - 12 - (int)strlen(buf) * 8, (HEADER_H - 16) / 2, COL_ACCENT, buf);
    wm_fill(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H - FOOTER_H, COL_BG);
    int disp = 0;
    for (int i = 0; i < zircapp_count(); i++) {
        zircapp_app_t *a = zircapp_get(i); int x, y, w, h; if (!a) continue;
        if (!store_is_installed(a->name)) continue;
        cell_rect(disp++, &x, &y, &w, &h);
        wm_fill(x, y, w, h, (i == active_app_idx) ? COL_CELL_HI : COL_CELL);
        wm_fill(x, y, w, 2, COL_ACCENT);
        wm_text(x + 10, y + (CELL_H - 16) / 2, COL_TEXT, a->name);
    }
    draw_status(); wm_flush();
}
static void module_fill_rect(int x, int y, int w, int h, uint32_t color) {
    wm_fill(x, y, w, h, color);
}
static void module_draw_text(int x, int y, uint32_t color, const char *text) {
    wm_text(x, y, color, text);
}
static void draw_app_screen(void) {
    zircapp_app_t *a = zircapp_get(active_app_idx);
    if (!a) { mode = MODE_HOME; active_app_idx = -1; draw_home(); return; }
    wm_fill(0, 0, SCREEN_W, HEADER_H, COL_BAR);
    wm_text(12, (HEADER_H - 16) / 2, COL_ACCENT,
            !strcmp(a->name, "Store") && store_detail_idx >= 0 ? "< Store" : "< Home");
    wm_text(96, (HEADER_H - 16) / 2, COL_TEXT, a->name);
    wm_fill(0, HEADER_H, SCREEN_W, SCREEN_H - HEADER_H - FOOTER_H, COL_BG);
    const char *n = a->name;
    if (a->render) {
        zircapp_canvas_t canvas = {
            .x = 0,
            .y = HEADER_H,
            .width = SCREEN_W,
            .height = SCREEN_H - HEADER_H - FOOTER_H,
            .fill_rect = module_fill_rect,
            .draw_text = module_draw_text,
        };
        zircapp_render(a, &canvas);
    }
    else if (!strcmp(n, "Calculator"))  draw_calc();
    else if (!strcmp(n, "Kall"))        draw_kall();
    else if (!strcmp(n, "Notes"))       draw_notes();
    else if (!strcmp(n, "Terminal"))    draw_terminal();
    else if (!strcmp(n, "Music"))       draw_music();
    else if (!strcmp(n, "Settings"))    draw_settings();
    else if (!strcmp(n, "SMS"))         draw_sms();
    else if (!strcmp(n, "Browser"))    draw_browser_app();
    else if (!strcmp(n, "Store"))       draw_store();
    else if (!strcmp(n, "Files"))       draw_files();
    else if (!strcmp(n, "Move On"))     draw_moveon();
    else {
        wm_text(16, HEADER_H + 22, COL_TEXT, "ZircApp app running under Zircon");
        wm_text(16, HEADER_H + 46, COL_DIM, "touch events broadcast to all apps");
        char buf[64]; snprintf(buf, sizeof(buf), "registry index %d", active_app_idx); wm_text(16, HEADER_H + 76, COL_FAINT, buf);
        snprintf(buf, sizeof(buf), "gui window id %d", a->win_id); wm_text(16, HEADER_H + 100, COL_FAINT, buf);
    }
    draw_status(); wm_flush();
}

/* Route a click inside the app body to the active app's screen. */
static void app_click(int x, int y) {
    const char *n = active_name();
    if (!strcmp(n, "Calculator")) { char k = calc_key_at(x, y); if (k) { calc_input(k); draw_app_screen(); } }
    else if (!strcmp(n, "Kall")) { char k = dial_key_at(x, y); if (k) { dial_input(k); draw_app_screen(); } else if (dial_call_at(x, y)) { if (ast.calling) ast.calling = 0; else if (ast.dial[0]) ast.calling = 1; draw_app_screen(); } }
    else if (!strcmp(n, "Settings")) { int row = settings_row_at(y); if (row >= 0) { settings_toggle(row); draw_app_screen(); } }
    else if (!strcmp(n, "Notes")) {
        int key = notes_touch_key(x, y);
        if (key == -1) notes_save();
        else if (key) note_input(key);
        draw_app_screen();
    }
    else if (!strcmp(n, "Files")) {
        if (y >= files_controls_y()) {
            files_button(x, y);
        } else if (ast.files_previewing) {
            ast.files_previewing = 0;
        } else if (y >= files_list_y()) {
            int row = (y - files_list_y()) / FILES_ROW_H;
            files_open_item(ast.files_scroll + row);
        } else {
            files_refresh();
        }
        draw_app_screen();
    }
    else if (!strcmp(n, "Music")) { if (music_button_at(x, y)) { music_toggle(); draw_app_screen(); } }
    else if (!strcmp(n, "Camera")) { ast.shots++; draw_app_screen(); }
    else if (!strcmp(n, "Gallery")) { int t = gallery_tile_at(x, y); if (t >= 0) { ast.tile_sel = t; draw_app_screen(); } }
    else if (!strcmp(n, "SMS")) { int t = sms_row_at(y); if (t >= 0) { ast.sms_sel = t; draw_app_screen(); } }
    else if (!strcmp(n, "Store")) { store_tap(x, y); draw_app_screen(); }
}

static void goto_home(void) {
    zircapp_background_all();
    mode = MODE_HOME;
    active_app_idx = -1;
    draw_home();
}
static void do_launch(int idx) {
    zircapp_app_t *a = zircapp_get(idx); if (!a) return;
    if (zircapp_activate(a->name) < 0)
        return;
    active_app_idx = idx; mode = MODE_APP; app_state_reset();
    printf("zircapp: launch '%s' (index %d)\n", a->name, idx);
    if (!strcmp(a->name, "Notes")) notes_load();
    else if (!strcmp(a->name, "Files")) files_load_home();
    draw_app_screen();
}

static void on_mouse(int x, int y, int pressed) {
    if (pressed) {
        zircapp_event_t ev; memset(&ev, 0, sizeof(ev));
        ev.type = ZIRCAPP_EVENT_TOUCH_DOWN; ev.mx = x; ev.my = y; ev.win_id = win_handle;
        ev.touch_count = 1; ev.touch[0].id = 0; ev.touch[0].x = x; ev.touch[0].y = y; ev.touch[0].pressure = 1;
        zircapp_broadcast(&ev);
        if (mode == MODE_HOME) { int idx = hit_cell(x, y); if (idx >= 0) do_launch(idx); }
        else if (x < 96 && y < HEADER_H) {
            if (mode == MODE_APP && !strcmp(active_name(), "Store") &&
                store_detail_idx >= 0) {
                store_detail_idx = -1;
                draw_app_screen();
            } else {
                goto_home();
            }
        }
        else {
            zircapp_app_t *app = zircapp_get(active_app_idx);
            if (app && app->render)
                draw_app_screen();
            else
                app_click(x, y);
        }
    } else {
        zircapp_event_t ev; memset(&ev, 0, sizeof(ev));
        ev.type = ZIRCAPP_EVENT_TOUCH_UP; ev.win_id = win_handle; zircapp_broadcast(&ev);
    }
}
static void on_key(int key) {
    zircapp_event_t ev; memset(&ev, 0, sizeof(ev));
    ev.type = ZIRCAPP_EVENT_KEY_DOWN; ev.key = key; ev.win_id = win_handle; zircapp_broadcast(&ev);
    if (key == 27) {
        if (mode == MODE_APP && !strcmp(active_name(), "Store") &&
            store_search_active) {
            store_search_active = 0;
            draw_app_screen();
        } else if (mode == MODE_APP && !strcmp(active_name(), "Store") &&
                   store_detail_idx >= 0) {
            store_detail_idx = -1;
            draw_app_screen();
        } else if (mode == MODE_APP) {
            goto_home();
        } else {
            have_window = 0;
        }
    }
    else if (mode == MODE_APP) {
        zircapp_app_t *app = zircapp_get(active_app_idx);
        if (app && app->render)
            draw_app_screen();
        else
            app_key(key);
    }
}

static int hit_cell(int px, int py) {
    int n = zircapp_count();
    for (int d = 0; d < n; d++) {
        int x, y, w, h; cell_rect(d, &x, &y, &w, &h);
        if (px >= x && px < x + w && py >= y && py < y + h) return home_registry_at(d);
    }
    return -1;
}
static void app_key(int key) {
    const char *n = active_name();
    if (!strcmp(n, "Calculator")) { calc_input(key < 256 ? key : 0); draw_app_screen(); }
    else if (!strcmp(n, "Notes")) { note_input(key < 256 ? key : 0); draw_app_screen(); }
    else if (!strcmp(n, "Kall")) { dial_input(key < 256 ? key : 0); draw_app_screen(); }
    else if (!strcmp(n, "Terminal")) { term_input(key < 256 ? key : 0); draw_app_screen(); }
    else if (!strcmp(n, "Store") && store_search_active) {
        store_search_input(key);
        draw_app_screen();
    }
}

static void handle_event(const uint8_t *p) {
    switch (p[0]) {
    case WM_EVENT_CONN:
        win_handle = p[1];
        have_window = 1;
        /* Tell the bridge how big this device's pixels are, and that this
         * client owns the whole panel: the kernel then clears the backdrop
         * around the interface, so a letterboxed phone layout on a bigger
         * panel is framed rather than surrounded by stale framebuffer. */
        {
            uint8_t sc[4] = { WM_SET_SCALE, (uint8_t)win_handle,
                               (uint8_t)DEVICE_SCALE, WM_FLAG_FULLSCREEN };
            wm_send(sc, 4);
        }
        break;
    case WM_EVENT_CLOSED: have_window = 0; break;
    case WM_EVENT_KEY: { uint32_t k = (uint32_t)p[2] | ((uint32_t)p[3] << 8) | ((uint32_t)p[4] << 16) | ((uint32_t)p[5] << 24); on_key((int)k); break; }
    case WM_EVENT_MOUSE: { int x = (int)(int16_t)((uint16_t)p[2] | ((uint16_t)p[3] << 8)); int y = (int)(int16_t)((uint16_t)p[4] | ((uint16_t)p[5] << 8)); on_mouse(x, y, p[6] != 0); break; }
    default: break;
    }
}

static void pump_events(void) {
    uint8_t buf[128]; int n = sys_read(WM_PIPE_EVENT, buf, (int)sizeof(buf)); int off = 0;
    while (off < n) {
        int left = n - off; int len = 0;
        switch (buf[off]) { case WM_EVENT_CONN: case WM_EVENT_CLOSED: len = 2; break; case WM_EVENT_KEY: len = 6; break; case WM_EVENT_MOUSE: len = 8; break; default: off = n; continue; }
        if (len > left) break; handle_event(buf + off); off += len;
    }
}

static int run_windowed(void) {
    wm_create(SCREEN_W, SCREEN_H, "Zircon");
    for (int waited = 0; waited < 2000 && !have_window; waited += 10) { pump_events(); if (!have_window) usleep(10000); }
    if (!have_window) return 0;
    mode = MODE_HOME; active_app_idx = -1; draw_home();
    int tick_acc = 0;
    while (have_window) {
        pump_events(); usleep(16000); tick_acc += 16;
        if (tick_acc >= 500) { tick_acc = 0; zircapp_event_t ev; memset(&ev, 0, sizeof(ev)); ev.type = ZIRCAPP_EVENT_TICK; zircapp_broadcast(&ev); if (mode == MODE_APP) { if (!strcmp(active_name(), "Music")) music_tick(); draw_app_screen(); } }
    }
    wm_close(); return 1;
}

/* Console session (no compositor, or --selftest) */
static const char *kSubApps[] = { "Kall", "SMS", "Settings", "Camera", "Clock", "Calculator", "Music", "Gallery", "Browser" };
static int run_console(int selftest) {
    printf("zircon: shell console mode\n"); for (int i = 0; i < zircapp_count(); i++) { zircapp_app_t *a = zircapp_get(i); if (a) printf("zircapp: app[%d] = '%s'\n", i, a->name); }
    if (!selftest) return 1;
    int launched = 0;
    for (unsigned i = 0; i < sizeof(kSubApps) / sizeof(kSubApps[0]); i++) { if (zircapp_launch(kSubApps[i]) >= 0) launched++; else printf("zircapp: launch failed '%s'\n", kSubApps[i]); }
    printf("zircapp: selftest launch %d/%d\n", launched, (int)(sizeof(kSubApps) / sizeof(kSubApps[0])));
    zircapp_event_t ev; memset(&ev, 0, sizeof(ev)); ev.type = ZIRCAPP_EVENT_TOUCH_DOWN; ev.touch_count = 1; ev.touch[0].pressure = 1; zircapp_broadcast(&ev);
    memset(&ev, 0, sizeof(ev)); ev.type = ZIRCAPP_EVENT_KEY_DOWN; ev.key = 'a'; zircapp_broadcast(&ev);
    memset(&ev, 0, sizeof(ev)); ev.type = ZIRCAPP_EVENT_TICK; zircapp_broadcast(&ev);
    printf("zircapp: selftest apps=%d launched=%d\n", zircapp_count(), launched);
    int notes_idx = zircapp_activate("Notes");
    int same_notes_idx = zircapp_activate("Notes");
    printf("zircapp: resume Notes index %d -> %d\n",
           notes_idx, same_notes_idx);

    /* The app store: the catalogue and the installed set are the store
     * package's own state, and the Home grid reads them, so the selftest
     * drives them here.  This is the headless half of the store check -- the
     * screen itself needs a framebuffer, but the logic does not. */
    int catalogue = store_count();
    int removed = store_set_installed("Clock", 0);
    int hidden = store_is_installed("Clock");
    int installed = store_set_installed("Clock", 1);
    int shown = store_is_installed("Clock");
    printf("zircapp: selftest store catalogue=%d remove=%d hidden=%d install=%d shown=%d\n",
           catalogue, removed, hidden, installed, shown);
    int store_ok = catalogue > 0 && removed == 1 && hidden == 0 && installed == 1 && shown == 1;

    return launched == (int)(sizeof(kSubApps) / sizeof(kSubApps[0])) &&
           notes_idx >= 0 && same_notes_idx == notes_idx &&
           store_ok;
}

/* ── Main ── */
int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("╔══════════════════════════════════════════╗\n");
    printf("║         Zircon OS v1.0.0 (Mobile)          ║\n");
    printf("║   Google-free mobile/desktop OS            ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    /* Which device is this?
     *
     * The kernel owns the device table (kernel/device.h): it knows the panel,
     * the logical screen the interface should be laid out for, the scale
     * between them and what the device can do.  Asking it is better than
     * guessing from the framebuffer, because the framebuffer says nothing
     * about the density or the form factor.
     *
     * A kernel without device support falls back to the framebuffer, which
     * is the old behaviour: a 1:1 screen with no scale. */
    zircon_device_info_t dev;
    if (zs_device_info(&dev) == 0 && dev.ui_w > 0 && dev.ui_h > 0) {
        SCREEN_W = (int)dev.ui_w;
        SCREEN_H = (int)dev.ui_h;
        DEVICE_SCALE = dev.scale > 0 ? (int)dev.scale : 1;
        DEVICE_FORM  = (int)dev.form;
        DEVICE_CAPS  = (int)dev.caps;
        printf("zircon: device %s (%s) %dx%d logical, %ux%u panel, %dx scale\n",
               dev.name, device_form_name(DEVICE_FORM),
               SCREEN_W, SCREEN_H, dev.panel_w, dev.panel_h, DEVICE_SCALE);
    } else {
        zircon_fb_info_t fb;
        if (zs_fb_info(&fb) == 0 && fb.width > 0 && fb.height > 0) {
            SCREEN_W = (int)fb.width;
            SCREEN_H = (int)fb.height;
        } else {
            SCREEN_W = 360;
            SCREEN_H = 640;
        }
        printf("zircon: device unknown, using the framebuffer %dx%d\n", SCREEN_W, SCREEN_H);
    }

    /* One icon per ~200 logical pixels: two on a phone, four to six on a
     * tablet. */
    COLS = SCREEN_W / 200;
    if (COLS < 2) COLS = 2;
    if (COLS > 6) COLS = 6;

    zircapp_phone_set_screen(SCREEN_W, SCREEN_H);

    gui_desktop_init(SCREEN_W, SCREEN_H);
    gui_window_init(); zircapp_init();

    zircapp_phone_init(); zircon_apps_register();

    printf("Zircon ready — %d app(s) registered\n", zircapp_count());
    printf("Zircon %s mode: %dx%d logical screen, %d columns, %dx scale\n",
           device_form_name(DEVICE_FORM), SCREEN_W, SCREEN_H, COLS, DEVICE_SCALE);

    if (sys_wm_attach() >= 0 && run_windowed()) return 0;
    printf("zircapp: WM bridge unavailable, running headless\n");
    int ok = run_console(0);
    printf("zircapp: done status=%d\n", ok ? 0 : 1);
    return ok ? 0 : 1;
}