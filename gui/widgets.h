#ifndef GUI_WIDGETS_H
#define GUI_WIDGETS_H

#include <stdint.h>

typedef void (*gui_draw_fn)(int x, int y, int w, int h, uint32_t color);

typedef struct {
    int x, y, w, h;
    char label[32];
    uint32_t fg;
    uint32_t bg;
    int pressed;
} gui_button_t;

typedef struct {
    int x, y, w, h;
    char text[64];
    uint32_t fg;
    uint32_t bg;
} gui_label_t;

void gui_widget_draw_button(gui_draw_fn draw, const gui_button_t *btn);
void gui_widget_draw_label(gui_draw_fn draw, const gui_label_t *lbl);

#endif
