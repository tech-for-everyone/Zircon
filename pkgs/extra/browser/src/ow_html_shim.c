/* ow_html_shim.c — C-owned OpenWeb render globals for Zircon.
 *
 * On CodeOS/Qt these live in qt6/panels/ow_html.c alongside kernel-backed
 * image workers.  Zircon runs against the same Rust renderer (libow_http.a →
 * ow_render_rs), which fills these extern statics by symbol, so we define the
 * globals here and provide self-contained form/image services (no kernel net).
 */
#include "ow_html.h"
#include <string.h>

/* ── Render output globals (populated by Rust ow_render_rs) ── */
char ow_txt[OW_TXT_LINES][OW_TXT_COLS];
int  ow_txt_lines;
ow_link_t ow_links[OW_MAX_LINKS];
int  ow_link_cnt;
ow_image_t ow_images[OW_MAX_IMAGES];
int  ow_image_cnt;
int  ow_need_render;
ow_line_info_t ow_line_info[OW_TXT_LINES];
int  ow_line_img[OW_TXT_LINES];
char ow_page_title[OW_URL_MAX];
/* <meta name="description"> / og:description text ("" if none) -- the
 * renderer writes this alongside ow_page_title since the OpenWeb meta work. */
char ow_page_desc[OW_URL_MAX];

ow_form_t ow_forms[OW_MAX_FORMS];
int  ow_form_cnt;
ow_form_field_t ow_form_fields[OW_MAX_FIELDS];
int  ow_field_cnt;

/* The form API (ow_field_set_value/toggle/at, ow_form_build_query) and the
 * persistent per-control edit cache (ow_fv_restore/ow_fv_store) live in the
 * Rust renderer now (kernel/kernel/rust_ow/src/ow_form.rs) and are exported
 * from libow_http.a.  Defining them here as well would collide with the
 * archive members this link pulls in for the render globals above. */

/* ── Image services (no network on Zircon/qemu — offline stubs) ── */

int ow_image_download(const char *url, void *buf, int max_len) {
    (void)url; (void)buf; (void)max_len;
    return -1;
}

void ow_image_start_workers(void) {}
void ow_image_enqueue(int idx, const char *abs_url) {
    (void)idx; (void)abs_url;
}
int ow_image_state(int idx) {
    (void)idx;
    return 0;
}
const unsigned char *ow_image_raw(int idx) {
    (void)idx;
    return 0;
}
int ow_image_raw_len(int idx) {
    (void)idx;
    return 0;
}
void ow_image_reset_all(void) {}