/* Minimal widget drawing helpers for modular GUI tree. */

#include "widgets.h"

static void fill_rect(gui_draw_fn draw, int x, int y, int w, int h, uint32_t color) {
    if (!draw)
        return;
    draw(x, y, w, h, color);
}

void gui_widget_draw_button(gui_draw_fn draw, const gui_button_t *btn) {
    if (!btn)
        return;
    uint32_t bg = btn->pressed ? (btn->bg & 0xFF000000) | ((btn->bg >> 1) & 0x00FFFFFF) : btn->bg;
    fill_rect(draw, btn->x, btn->y, btn->w, btn->h, bg);
    fill_rect(draw, btn->x, btn->y, btn->w, 2, btn->fg);
    fill_rect(draw, btn->x, btn->y + btn->h - 2, btn->w, 2, btn->fg);
}

void gui_widget_draw_label(gui_draw_fn draw, const gui_label_t *lbl) {
    if (!lbl)
        return;
    fill_rect(draw, lbl->x, lbl->y, lbl->w, lbl->h, lbl->bg);
}
