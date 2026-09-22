/* OpenWeb C bridge — bridges the Rust HTTP/render backend (libow_http.a)
 * and the C render-output globals (qt6/panels/ow_html.c) with Zircon. */
#include "ow_html.h"
#include "ow_http.h"
#include "openweb.h"
#include <stdio.h>
#include <string.h>

/* ── Rust backend exports (ow_http.rs → libow_http.a) ──
 * These are declared in qt6/panels/ow_http.h already; the renderer entry
 * point below is declared in qt6/panels/ow_html.h. */

/* ── Navigation ── */

void browser_navigate_to(const char *url) {
    if (!url) return;
    ow_navigate(url);
}

void browser_navigate_post_form(const char *action, const void *body, int body_len) {
    ow_navigate_post(action, body, body_len);
}

void browser_navigate_fresh(const char *url) {
    ow_navigate_fresh(url);
}

void browser_search_web(const char *query) {
    ow_search(query);
}

/* ── Tab management ── */

int browser_get_tab_count(void) {
    return ow_get_tab_count();
}

int browser_get_active_tab(void) {
    return ow_get_tab_active();
}

void browser_set_active_tab(int idx) {
    ow_set_tab_active(idx);
}

int browser_get_used_tabs(void) {
    return ow_tab_used_count();
}

/* ── Rendering ── */

void browser_render(const char *html, int len) {
    ow_render_rs(html, len);
}

int browser_need_render(void) {
    return ow_need_render;
}

void browser_clear_render_flag(void) {
    ow_need_render = 0;
}

/* ── Status ── */

const char *browser_get_status(void) {
    openweb_tab_t *tabs = ow_get_tabs();
    int active = ow_get_tab_active();
    if (active >= 0 && active < ow_get_tab_count()) {
        return (const char *)tabs[active].status;
    }
    return "No tabs";
}

int browser_get_load_progress(void) {
    return ow_get_load_progress();
}

const char *browser_get_page_title(void) {
    return (const char *)ow_page_title;
}

/* ── Content access ── */

int browser_txt_lines(void) {
    return ow_txt_lines;
}

const char *browser_txt_line(int ln) {
    if (ln < 0 || ln >= OW_TXT_LINES) return "";
    return (const char *)ow_txt[ln];
}

const ow_link_t *browser_links(int *count) {
    if (count) *count = ow_link_cnt;
    return ow_links;
}

/* ── Image services ── */

int browser_download_image(const char *url, void *buf, int max_len) {
    return ow_image_download(url, buf, max_len);
}

void browser_image_start_workers(void) {
    ow_image_start_workers();
}

/* ── Form services ── */

void browser_set_field_value(int idx, const char *value) {
    ow_field_set_value(idx, value);
}

void browser_toggle_field(int idx) {
    ow_field_toggle(idx);
}

int browser_field_at(int line, int col) {
    return ow_field_at(line, col);
}

int browser_build_query(const ow_form_t *f, char *out, int out_max) {
    return ow_form_build_query(f, out, out_max);
}