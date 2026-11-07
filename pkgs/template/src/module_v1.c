#include <zircapp_module_v1.h>

static void example_event(const zircapp_module_event_v1_t *event) {
    if (event && event->type == ZIRCAPP_MODULE_EVENT_TOUCH_DOWN) {
        volatile int last_x = event->x;
        (void)last_x;
    }
}

const zircapp_module_v1_t *zircon_app_module_v1(void) {
    static const char name[] = "C Example";
    static const zircapp_module_v1_t module = {
        .abi_version = ZIRCAPP_MODULE_ABI_V1,
        .struct_size = sizeof(zircapp_module_v1_t),
        .name = name,
        .name_len = sizeof(name) - 1,
        .event = example_event,
    };
    return &module;
}
