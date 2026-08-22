/* Lightweight window registry for modular GUI tree. Kernel GUI: kernel/desktop.c */

#include "windows.h"

static gui_window_t windows[GUI_MAX_WINDOWS];
static int win_count;

void gui_window_init(void) {
    win_count = 0;
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        windows[i].visible = 0;
}

int gui_window_create(int x, int y, int w, int h, const char *title) {
    if (win_count >= GUI_MAX_WINDOWS)
        return -1;
    int id = win_count++;
    gui_window_t *win = &windows[id];
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->bg = 0xFF2a2a3e;
    win->border = 0xFF00cc44;
    win->visible = 1;
    win->focused = 0;

    int i = 0;
    if (title) {
        while (title[i] && i < GUI_TITLE_MAX - 1) {
            win->title[i] = title[i];
            i++;
        }
    }
    win->title[i] = 0;
    return id;
}

void gui_window_destroy(int id) {
    if (id < 0 || id >= win_count)
        return;
    windows[id].visible = 0;
}

void gui_window_move(int id, int x, int y) {
    if (id < 0 || id >= win_count)
        return;
    windows[id].x = x;
    windows[id].y = y;
}

gui_window_t *gui_window_get(int id) {
    if (id < 0 || id >= win_count || !windows[id].visible)
        return 0;
    return &windows[id];
}

int gui_window_count(void) {
    return win_count;
}
