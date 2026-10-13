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

ow_form_t ow_forms[OW_MAX_FORMS];
int  ow_form_cnt;
ow_form_field_t ow_form_fields[OW_MAX_FIELDS];
int  ow_field_cnt;

/* ── Form field edits (values survive re-render via Rust fv bridge) ── */

void ow_field_set_value(int i, const char *v) {
    if (i < 0 || i >= ow_field_cnt || !v) return;
    strncpy(ow_form_fields[i].value, v, OW_URL_MAX - 1);
    ow_form_fields[i].value[OW_URL_MAX - 1] = 0;
}

void ow_field_toggle(int i) {
    if (i < 0 || i >= ow_field_cnt) return;
    ow_form_fields[i].checked = ow_form_fields[i].checked ? 0 : 1;
}

int ow_field_at(int line, int col) {
    for (int i = 0; i < ow_field_cnt; i++) {
        ow_form_field_t *f = &ow_form_fields[i];
        if (f->line == line && col >= f->col && col < f->col + f->width)
            return i;
    }
    return -1;
}

int ow_form_build_query(const ow_form_t *f, char *out, int out_max) {
    if (!f || !out || out_max <= 0) return 0;
    int used = 0;
    for (int i = 0; i < f->field_count; i++) {
        int idx = f->field_start + i;
        if (idx < 0 || idx >= ow_field_cnt) continue;
        ow_form_field_t *fi = &ow_form_fields[idx];
        if (fi->type == OW_FT_BUTTON || fi->type == OW_FT_SUBMIT) continue;
        if ((fi->type == OW_FT_CHECKBOX || fi->type == OW_FT_RADIO) && !fi->checked)
            continue;
        int need = (int)strlen(fi->name) + (int)strlen(fi->value) + 2;
        if (used + need >= out_max - 1) break;
        if (used) out[used++] = '&';
        strcpy(out + used, fi->name);
        used += (int)strlen(fi->name);
        out[used++] = '=';
        strcpy(out + used, fi->value);
        used += (int)strlen(fi->value);
    }
    out[used] = 0;
    return used;
}

void ow_fv_restore(ow_form_field_t *f, int fi) {
    (void)f; (void)fi; /* edits live in our globals already; nothing to bridge */
}

void ow_fv_store(const ow_form_field_t *f, int fi) {
    (void)f; (void)fi;
}

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