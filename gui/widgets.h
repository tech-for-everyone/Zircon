/* Zircon widgets header — references kernel GUI as the single source of truth */
#ifndef ZIRCON_WIDGETS_H
#define ZIRCON_WIDGETS_H

#include "desktop.h"
#include <stdint.h>

/* Draw callback type */
typedef void (*gui_draw_fn)(int x, int y, int w, int h, uint32_t color);

/* Button widget */
typedef struct {
    int x, y, w, h;
    uint32_t fg;
    uint32_t bg;
    int pressed;
} gui_button_t;

/* Label widget */
typedef struct {
    int x, y, w, h;
    uint32_t fg;
    uint32_t bg;
    const char *text;
} gui_label_t;

/* Widget drawing helpers */
void gui_widget_draw_button(gui_draw_fn draw, const gui_button_t *btn);
void gui_widget_draw_label(gui_draw_fn draw, const gui_label_t *lbl);

#endif
