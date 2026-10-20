/* Zircon panels header — references kernel GUI as the single source of truth */
#ifndef ZIRCON_PANELS_H
#define ZIRCON_PANELS_H

#include "desktop.h"

/* Desktop panel management */
void gui_desktop_init(int width, int height);
void gui_desktop_set_bg(uint32_t color);
int gui_desktop_width(void);
int gui_desktop_height(void);

#endif
