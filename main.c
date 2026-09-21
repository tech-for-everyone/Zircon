/* Zircon — the Better Android
 * Main entry point. Runs as a userspace service on CodeOS,
 * connects to kernel via IPC, manages apps and the launcher. */

#include "gui/panels.h"
#include "gui/windows.h"
#include "gui/widgets.h"
#include "apps/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fcntl.h>

/* ── Kernel IPC bridge ── */

#define ZIRCON_IPC_MAGIC  0x5A495243

typedef enum {
    ZIRCON_IPC_NONE = 0,
    ZIRCON_IPC_NOTIFY,
    ZIRCON_IPC_APP_LAUNCH,
    ZIRCON_IPC_APP_CLOSE,
    ZIRCON_IPC_TOUCH_EVENT,
    ZIRCON_IPC_CMD_RESP,
    ZIRCON_IPC_APP_LIST,
    ZIRCON_IPC_QS_TOGGLE,
} zircon_ipc_type_t;

typedef struct {
    zircon_ipc_type_t type;
    int x, y, w, h;
    int id;
    char text[128];
} zircon_ipc_msg_t;

typedef struct {
    uint32_t magic;
    volatile uint32_t head;
    volatile uint32_t tail;
    zircon_ipc_msg_t msgs[16];
} zircon_ipc_ring_t;

static zircon_ipc_ring_t *ipc_ring;

static int zircon_ipc_init(void) {
    /* Get shared page physical address from kernel */
    long phys = syscall(7, 0, 0, 0, 0, 0);
    if (phys <= 0) {
        fprintf(stderr, "zircon: no kernel IPC\n");
        return -1;
    }
    /* Map it */
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0) return -1;
    ipc_ring = (zircon_ipc_ring_t *)mmap(0, 4096, PROT_READ|PROT_WRITE,
                                          MAP_SHARED, fd, phys);
    close(fd);
    if (ipc_ring == MAP_FAILED || ipc_ring->magic != ZIRCON_IPC_MAGIC) {
        ipc_ring = 0;
        return -1;
    }
    printf("zircon: IPC connected at 0x%lx\n", phys);
    return 0;
}

static int zircon_ipc_poll(zircon_ipc_msg_t *msg) {
    if (!ipc_ring) return 0;
    if (ipc_ring->tail == ipc_ring->head) return 0;
    uint32_t t = ipc_ring->tail;
    *msg = ipc_ring->msgs[t];
    __sync_synchronize();
    ipc_ring->tail = (t + 1) % 16;
    return 1;
}

/* ── Notification handler ── */

static void handle_notify(const zircon_ipc_msg_t *msg) {
    printf("\033[1;34m[Zircon]\033[0m %s\n", msg->text);
}

/* ── Built-in app: Home Screen ── */

/* Forward declarations for app registration */
extern zircon_app_t home_app;

/* ── Main ── */

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("╔══════════════════════════════════════════╗\n");
    printf("║         Zircon OS v1.0.0 (Mobile)          ║\n");
    printf("║   Google-free mobile/desktop OS            ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    /* Connect to kernel via IPC */
    zircon_ipc_init();

    /* Set phone screen dimensions */
    zircon_phone_set_screen(360, 640);
    gui_desktop_init(zircon_phone_get_width(), zircon_phone_get_height());
    gui_window_init();
    zircon_app_init();

    /* Register phone apps */
    zircon_phone_init();
    zircon_app_register(&home_app);

    printf("Zircon ready — %d app(s) registered\n", zircon_app_count());
    printf("Zircon mobile mode: touch screen %dx%d\n", zircon_phone_get_width(), zircon_phone_get_height());

    zircon_app_launch("Home");

    /* Event loop with IPC polling and touch processing */
    zircon_event_t ev;
    zircon_ipc_msg_t ipc_msg;
    int running = 1;
    while (running) {
        /* Poll kernel IPC for phone events */
        while (zircon_ipc_poll(&ipc_msg)) {
            switch (ipc_msg.type) {
            case ZIRCON_IPC_NOTIFY:
                handle_notify(&ipc_msg);
                break;
            case ZIRCON_IPC_APP_LAUNCH: {
                /* Launch an app by name */
                zircon_app_t *app = zircon_app_find(ipc_msg.text);
                if (app) {
                    zircon_app_launch(app->name);
                    printf("launched %s\n", app->name);
                }
                break;
            }
            case ZIRCON_IPC_QS_TOGGLE:
                zircon_phone_draw_quick_settings();
                break;
            default:
                break;
            }
        }

        /* Process touch events */
        ev.type = ZIRCON_EVENT_TICK;
        zircon_app_broadcast(&ev);
    }

    printf("Zircon shutting down\n");
    return 0;
}
