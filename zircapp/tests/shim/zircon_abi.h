/* Host-test shim for Zircon's kernel ABI.
 *
 * The real Zircon/zircon_abi.h only declares the ABI types and the zs_*
 * wrappers (stubs.c implements them with `int $0x80`).  For the host test
 * we need the same types and the same zs_* prototypes, but the test file
 * provides the implementations, so nothing here executes a syscall.
 *
 * Keep this in sync with Zircon/zircon_abi.h for the handful of symbols
 * zircapp.c actually consumes: the IPC message type, the input event, and
 * the zs_ipc_query / zs_ipc_recv / zs_input_poll wrappers. */
#ifndef ZIRCON_ABI_H
#define ZIRCON_ABI_H

#include <stdint.h>

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
    uint64_t reserved0;
    int      type;
    int      key;
    int      mouse_x;
    int      mouse_y;
    int      mouse_buttons;
    uint64_t reserved1;
} zircon_input_ev_t;

long zs_ipc_query(void);
int  zs_ipc_recv(zircon_ipc_msg_t *msg);
int  zs_input_poll(zircon_input_ev_t *ev);

#endif /* ZIRCON_ABI_H */
