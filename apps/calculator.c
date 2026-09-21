/* Zircon Calculator — Native C calculator app */
#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include <stdio.h>
#include <string.h>

static double result = 0;
static char expression[64];
static int expr_len = 0;

static void calc_init(struct zircon_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircon_phone_get_width(), zircon_phone_get_height(), "Calculator");
    result = 0;
    expr_len = 0;
    expression[0] = '\0';
    printf("calc: initialized\n");
}

static void calc_event(struct zircon_app *app, const zircon_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCON_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int tx = ev->touch[0].x;
        int ty = ev->touch[0].y;
        int w = zircon_phone_get_width();
        int h = zircon_phone_get_height();
        int col = tx / (w / 3);
        int row = ty / (h / 5);
        const char *btn_labels[] = {"7","8","9","/","4","5","6","*","1","2","3","-","0",".","=","+"};
        if (row < 4 && col < 4) {
            int idx = row * 4 + col;
            if (idx >= 0 && idx < 16) {
                const char *label = btn_labels[idx];
                if (strcmp(label, "=") == 0) {
                    printf("calc: = result=%g\n", result);
                    expr_len = 0;
                } else if (strcmp(label, "C") == 0 || row == 0 && col == 0) {
                    result = 0;
                    expr_len = 0;
                    expression[0] = '\0';
                } else {
                    printf("calc: %s\n", label);
                }
            }
        }
    }
}

static void calc_stop(struct zircon_app *app) {
    (void)app;
    gui_window_close(app->win_id);
}

zircon_app_t calc_app = {
    .name = "Calculator",
    .init = calc_init,
    .event = calc_event,
    .stop = calc_stop,
    .state = ZIRCON_APP_STOPPED,
    .win_id = -1,
};
