#ifndef GUI_WINDOW_H
#define GUI_WINDOW_H

#include <stdint.h>

#define GUI_MAX_WINDOWS 16
#define GUI_TITLE_MAX   48

typedef struct {
    int x, y, w, h;
    char title[GUI_TITLE_MAX];
    uint32_t bg;
    uint32_t border;
    int visible;
    int focused;
} gui_window_t;

void gui_window_init(void);
int  gui_window_create(int x, int y, int w, int h, const char *title);
void gui_window_destroy(int id);
void gui_window_move(int id, int x, int y);
gui_window_t *gui_window_get(int id);
int  gui_window_count(void);

#endif
