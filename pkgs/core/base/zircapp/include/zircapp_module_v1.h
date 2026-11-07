#ifndef ZIRCAPP_MODULE_V1_H
#define ZIRCAPP_MODULE_V1_H

#include <stdint.h>

#define ZIRCAPP_MODULE_ABI_V1 1
#define ZIRCAPP_MODULE_EVENT_TOUCH_DOWN 10

typedef struct {
    uint32_t type;
    int32_t x;
    int32_t y;
    int32_t key;
} zircapp_module_event_v1_t;

typedef struct {
    uint32_t abi_version;
    uint32_t struct_size;
    const char *name;
    uint32_t name_len;
    void (*init)(void);
    void (*event)(const zircapp_module_event_v1_t *event);
    void (*stop)(void);
} zircapp_module_v1_t;

/* Register a statically linked C or Rust module through the stable v1 ABI.
 * The module name must be NUL-terminated at name_len. */
int zircapp_register_module_v1(const zircapp_module_v1_t *module);

#endif
