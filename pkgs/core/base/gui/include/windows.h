/* Zircon windows header — references kernel GUI as the single source of truth */
#ifndef ZIRCON_WINDOWS_H
#define ZIRCON_WINDOWS_H

#include "desktop.h"
#include <stdint.h>

#define GUI_MAX_WINDOWS 32
#define GUI_TITLE_MAX 128

/* Window structure */
typedef struct {
    int x, y, w, h;
    uint32_t fg;
    uint32_t bg;
    uint32_t border;
    int visible;
    int focused;
    char title[GUI_TITLE_MAX];
} gui_window_t;

/* GUI window management */
int gui_window_create(int x, int y, int w, int h, const char *title);
void gui_window_init(void);
void gui_window_show(int id);
void gui_window_close(int id);
gui_window_t *gui_window_get(int id);
int gui_window_count(void);

#endif
