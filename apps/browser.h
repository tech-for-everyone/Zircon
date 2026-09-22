#ifndef ZIRCON_BROWSER_H
#define ZIRCON_BROWSER_H

#include "app.h"
#include <openweb.h>
#include <ow_http.h>
#include <ow_html.h>

extern zircon_app_t browser_app;

void browser_navigate(const char *url);
void browser_new_tab(const char *url);
void browser_close_tab(int idx);
void browser_go_back(void);
void browser_go_forward(void);
const char *browser_get_url(void);
void browser_render_page(void);

#endif
