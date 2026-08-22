/* Modular desktop surface state. Kernel GUI: kernel/desktop.c */

#include "panels.h"

static int screen_w;
static int screen_h;
static uint32_t bg_color;

void gui_desktop_init(int width, int height) {
    screen_w = width > 0 ? width : 1024;
    screen_h = height > 0 ? height : 768;
    bg_color = 0xFF1a1a2e;
}

void gui_desktop_set_bg(uint32_t color) {
    bg_color = color;
}

int gui_desktop_width(void) {
    return screen_w;
}

int gui_desktop_height(void) {
    return screen_h;
}
