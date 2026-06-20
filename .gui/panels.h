#ifndef GUI_DESKTOP_H
#define GUI_DESKTOP_H

#include <stdint.h>

void gui_desktop_init(int width, int height);
void gui_desktop_set_bg(uint32_t color);
int  gui_desktop_width(void);
int  gui_desktop_height(void);

#endif
