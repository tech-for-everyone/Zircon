/* Zircon Terminal — a minimal command line.  Keys accumulate into a buffer
 * and a tap (or Enter) runs a couple of built-ins. */
#include <zircapp.h>
#include <windows.h>
#include <widgets.h>
#include <string.h>
#include <stdio.h>

#define TERM_LINE_MAX 128

static char line[TERM_LINE_MAX];
static int line_len;
static int commands_run;

static void terminal_run(void) {
    line[line_len] = '\0';
    if (line_len == 0)
        return;
    if (strcmp(line, "help") == 0)
        printf("terminal: commands: help, version, clear\n");
    else if (strcmp(line, "version") == 0)
        printf("terminal: Zircon OS v1.0.0\n");
    else if (strcmp(line, "clear") == 0)
        commands_run = 0;
    else
        printf("terminal: %s: command not found\n", line);
    line_len = 0;
    line[0] = '\0';
    commands_run++;
}

static void terminal_init(struct zircapp_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircapp_phone_get_width(),
                                    zircapp_phone_get_height(), "Terminal");
    line[0] = '\0';
    line_len = 0;
    commands_run = 0;
    printf("terminal: ready\n");
}

static void terminal_event(struct zircapp_app *app, const zircapp_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCAPP_EVENT_KEY_DOWN) {
        if (ev->key == '\n' || ev->key == '\r') {
            terminal_run();
        } else if (line_len < TERM_LINE_MAX - 1 &&
                   ev->key >= 0x20 && ev->key < 0x7f) {
            line[line_len++] = (char)ev->key;
        }
    } else if (ev->type == ZIRCAPP_EVENT_TOUCH_DOWN) {
        terminal_run();
    }
}

static void terminal_stop(struct zircapp_app *app) {
    gui_window_close(app->win_id);
}

zircapp_app_t terminal_app = {
    .name = "Terminal",
    .init = terminal_init,
    .event = terminal_event,
    .stop = terminal_stop,
    .state = ZIRCAPP_APP_STOPPED,
    .win_id = -1,
};
