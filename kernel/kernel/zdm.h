/* Minimal ZDM stubs for Zircon */
#ifndef ZDM_H
#define ZDM_H

#include <stdint.h>

typedef enum {
    ZDM_INIT = 0,
    ZDM_NOTIFICATION = 1,
    ZDM_SHADE_TOGGLE = 2,
    ZDM_DRAWER_TOGGLE = 3,
    ZDM_QS_TOGGLE = 4,
} zdm_event_type_t;

typedef struct {
    zdm_event_type_t type;
    char text[128];
    uint64_t timestamp;
} zdm_event_t;

static inline void zdm_set_enabled(int en) { (void)en; }
static inline int zdm_is_enabled(void) { return 0; }
static inline void zdm_log(zdm_event_type_t type, const char *text) { (void)type; (void)text; }
static inline void zdm_dump(void) { }
static inline void zdm_clear(void) { }
static inline int zdm_event_count(void) { return 0; }
static inline const void *zdm_event_at(int idx) { (void)idx; return 0; }

#endif
