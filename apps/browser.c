/* Zircon Browser — OpenWeb integration for Zircon mobile OS */
#include "app.h"
#include "../gui/windows.h"
#include "../gui/widgets.h"
#include "browser.h"
#include <openweb.h>
#include <ow_http.h>
#include <ow_html.h>
#include <stdio.h>
#include <string.h>

static int browser_active = 0;
static char current_url[OW_URL_MAX];
static int show_url_bar = 1;

static void browser_draw_url_bar(int x, int y, int w, int h, const char *url) {
    (void)x; (void)y; (void)w; (void)h;
    /* Draw URL bar at top */
    printf("browser: %s\n", url);
}

static void browser_draw_page(int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
    /* Draw rendered HTML page */
    if (!browser_active) return;
    /* Use ow_txt global from ow_html.c */
    extern char ow_txt[OW_TXT_LINES][OW_TXT_COLS];
    extern int ow_txt_lines;
    for (int i = 0; i < ow_txt_lines && i < OW_TXT_LINES; i++) {
        if (ow_txt[i][0] != 0) {
            printf("%s\n", ow_txt[i]);
        }
    }
}

static void browser_draw_links(int x, int y, int w) {
    (void)x; (void)y; (void)w;
    extern ow_link_t ow_links[OW_MAX_LINKS];
    extern int ow_link_cnt;
    for (int i = 0; i < ow_link_cnt && i < OW_MAX_LINKS; i++) {
        printf("  [%d] %s\n", i, ow_links[i].url);
    }
}

static void browser_init(struct zircon_app *app) {
    (void)app;
    app->win_id = gui_window_create(0, 0, zircon_phone_get_width(), zircon_phone_get_height(), "Browser");
    browser_active = 1;
    current_url[0] = '\0';
    show_url_bar = 1;
    printf("browser: initialized\n");
}

static void browser_event(struct zircon_app *app, const zircon_event_t *ev) {
    (void)app;
    if (ev->type == ZIRCON_EVENT_TOUCH_DOWN && ev->touch_count > 0) {
        int tx = ev->touch[0].x;
        int ty = ev->touch[0].y;
        int w = zircon_phone_get_width();
        int h = zircon_phone_get_height();

        /* URL bar at top */
        if (ty < 40) {
            /* Touch URL bar area */
            show_url_bar = 1;
            printf("browser: URL bar touched\n");
        }
        /* Navigation buttons */
        else if (ty < 80) {
            if (tx < w / 4) {
                /* Back button */
                if (ow_get_tab_active() > 0) {
                    ow_set_tab_active(ow_get_tab_active() - 1);
                    browser_active = 1;
                    printf("browser: back\n");
                }
            } else if (tx < w / 2) {
                /* Forward button */
                if (ow_get_tab_active() < ow_tab_used_count() - 1) {
                    ow_set_tab_active(ow_get_tab_active() + 1);
                    browser_active = 1;
                    printf("browser: forward\n");
                }
            } else if (tx < w * 3 / 4) {
                /* Reload button */
                printf("browser: reload\n");
            } else {
                /* Home button */
                ow_tab_new("https://www.google.com");
                printf("browser: home\n");
            }
        }
        /* Page content area */
        else if (ty < h - 60) {
            /* Handle link clicks */
            extern ow_link_t ow_links[OW_MAX_LINKS];
            extern int ow_link_cnt;
            int row = (ty - 80) / 16;
            if (row >= 0 && row < ow_link_cnt) {
                printf("browser: clicked link %d: %s\n", row, ow_links[row].url);
            }
        }
        /* Bottom bar (URL input) */
        else {
            /* Touch to enter URL */
            printf("browser: bottom bar touched\n");
        }
    }

    if (ev->type == ZIRCON_EVENT_KEY_DOWN) {
        /* Handle keyboard navigation */
        if (ev->key == 13) { /* Enter */
            printf("browser: enter key\n");
        }
    }

    /* Handle web events */
    if (ev->type == ZIRCON_EVENT_TICK) {
        /* Update page rendering */
        if (browser_active) {
            /* ow_render_rs is called by the compositor to render the page */
        }
    }
}

static void browser_stop(struct zircon_app *app) {
    (void)app;
    gui_window_close(app->win_id);
    browser_active = 0;
}

zircon_app_t browser_app = {
    .name = "Browser",
    .init = browser_init,
    .event = browser_event,
    .stop = browser_stop,
    .state = ZIRCON_APP_STOPPED,
    .win_id = -1,
};

/* ── OpenWeb Bridge ── */

void browser_navigate(const char *url) {
    if (!url || !browser_active) return;
    strncpy(current_url, url, OW_URL_MAX - 1);
    current_url[OW_URL_MAX - 1] = '\0';
    ow_navigate(url);
    printf("browser: navigating to %s\n", url);
}

void browser_new_tab(const char *url) {
    if (!url) url = "https://www.google.com";
    ow_tab_new(url);
    printf("browser: new tab to %s\n", url);
}

void browser_close_tab(int idx) {
    ow_tab_close(idx);
    printf("browser: closed tab %d\n", idx);
}

void browser_go_back(void) {
    int active = ow_get_tab_active();
    if (active > 0) {
        ow_set_tab_active(active - 1);
        printf("browser: went back\n");
    }
}

void browser_go_forward(void) {
    int active = ow_get_tab_active();
    int count = ow_tab_used_count();
    if (active < count - 1) {
        ow_set_tab_active(active + 1);
        printf("browser: went forward\n");
    }
}

const char *browser_get_url(void) {
    return current_url;
}

void browser_render_page(void) {
    if (!browser_active) return;
    /* The page is rendered via ow_render_rs() in Rust */
    /* This just triggers the update */
}
