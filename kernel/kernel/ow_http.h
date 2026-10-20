/* Dummy ow_http.h for Zircon (no OpenWeb integration) */
#ifndef OW_HTTP_H
#define OW_HTTP_H

#include <stdint.h>

/* Dummy types for when OpenWeb is not available */
typedef struct {
    char url[512];
    char status[64];
    char content[4096];
    int content_len;
    int loading;
    int error;
    int can_go_back;
    int can_go_forward;
} openweb_tab_t;

/* web_state_t is defined in syscall.h */

/* Dummy function declarations */
static inline int ow_get_tab_count(void) { return 0; }
static inline int ow_tab_used_count(void) { return 0; }
static inline int ow_get_load_progress(void) { return 0; }
static inline openweb_tab_t* ow_get_tabs(void) { return 0; }
static inline int ow_get_tab_active(void) { return -1; }
static inline void ow_navigate(const char* s) { (void)s; }
static inline void ow_search(const char* s) { (void)s; }
static inline void ow_tab_new(const char* s) { (void)s; }
static inline void ow_tab_close(int n) { (void)n; }
static inline void ow_set_tab_active(int n) { (void)n; }
static inline void ow_go_back(void) { }
static inline void ow_go_forward(void) { }
static inline void ow_stop_loading(void) { }

#endif
