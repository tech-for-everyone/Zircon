/* ─────────────────────────────────────────────────────────────
 * Zircon — CodeOS kernel ABI
 *
 * Native CodeOS syscall numbers, IPC message/ring layouts and the
 * FB_INFO / INPUT_POLL payloads.  These mirror the kernel ABI in
 * include/codeos/syscall_abi.h so Zircon's userspace talks to the
 * kernel over real `int $0x80` syscalls instead of faked stubs.
 *
 * Syscall convention (x86_64):
 *   int $0x80;  RAX = syscall number
 *               RDI/RSI/RDX/R10/R8/R9 = a1..a6
 * ───────────────────────────────────────────────────────────── */
#ifndef ZIRCON_ABI_H
#define ZIRCON_ABI_H

#include <stdint.h>

/* ── Native CodeOS syscall numbers (stable ABI) ── */
#define SYSCALL_WRITE         0
#define SYSCALL_SLEEP         1
#define SYSCALL_EXIT          2
#define SYSCALL_GETTID        3
#define SYSCALL_OPEN          4
#define SYSCALL_READ          5
#define SYSCALL_YIELD         6
#define SYSCALL_ZIRCON_IPC    7
#define SYSCALL_FORK          8
#define SYSCALL_EXECVE        9
#define SYSCALL_WAIT         10
#define SYSCALL_GETPID       11
#define SYSCALL_GETPPID      12
#define SYSCALL_CLOSE        13
#define SYSCALL_PIPE         14
#define SYSCALL_DUP          15
#define SYSCALL_BRK          16
#define SYSCALL_MMAP         17
#define SYSCALL_MUNMAP       18
#define SYSCALL_LSEEK        19
#define SYSCALL_SBRK         20
#define SYSCALL_BINDER       21
#define SYSCALL_ASHMEM       22
#define SYSCALL_CONTAINER_CREATE  23
#define SYSCALL_CONTAINER_EXEC    24
#define SYSCALL_CONTAINER_DESTROY 25
#define SYSCALL_CONTAINER_LIST    26
#define SYSCALL_AI_QUERY          27
#define SYSCALL_FB_INFO           28
#define SYSCALL_INPUT_POLL        29
#define SYSCALL_SET_PERSONALITY   30
#define SYSCALL_AUDIO_PLAY        31
#define SYSCALL_AUDIO_STATUS      32
#define SYSCALL_PWRITE            33
#define SYSCALL_DUP2              34
#define SYSCALL_SHM               35
#define SYSCALL_GET_INFO          36
#define SYSCALL_WEB               37
#define SYSCALL_STAT              38
#define SYSCALL_CHDIR             39
#define SYSCALL_GETCWD            40
#define SYSCALL_MKDIR             41
#define SYSCALL_RMDIR             42
#define SYSCALL_UNLINK            43
#define SYSCALL_RENAME            44
#define SYSCALL_READDIR           45
#define SYSCALL_CHMOD             46
#define SYSCALL_ACCESS            47
#define SYSCALL_X11_CONNECT       48
#define SYSCALL_X11_REQUEST       49
#define SYSCALL_X11_POLL          50
#define SYSCALL_KILL              51
#define SYSCALL_VM                52
#define SYSCALL_TIME              53
#define SYSCALL_GETTIMEOFDAY      54
#define SYSCALL_CONTAINER_START   55
#define SYSCALL_AUDIO_OPEN        56
#define SYSCALL_AUDIO_WRITE       57
#define SYSCALL_AUDIO_CLOSE       58
#define SYSCALL_AUDIO_SET_VOLUME  59
#define SYSCALL_AUDIO_GET_VOLUME  60
#define SYSCALL_AUDIO_SET_STATE   61
#define SYSCALL_AUDIO_GET_POS     62
#define SYSCALL_AUDIO_GET_FREE    63
#define SYSCALL_AUDIO_SET_MASTER  64
#define SYSCALL_AUDIO_GET_MASTER  65
#define SYSCALL_SOCKET_SOCKET      66
#define SYSCALL_SOCKET_CLOSE       67
#define SYSCALL_SOCKET_BIND        68
#define SYSCALL_SOCKET_CONNECT     69
#define SYSCALL_SOCKET_SENDTO      70
#define SYSCALL_SOCKET_RECVFROM    71
#define SYSCALL_SOCKET_SEND        72
#define SYSCALL_SOCKET_RECV        73
#define SYSCALL_SOCKET_GETSOCKNAME 74
#define SYSCALL_SOCKET_GETPEERNAME 75
#define SYSCALL_SOCKET_SETSOCKOPT  76
#define SYSCALL_SOCKET_GETSOCKOPT  77
#define SYSCALL_SOCKET_LISTEN      78
#define SYSCALL_SOCKET_ACCEPT      79
#define SYSCALL_WM_ATTACH          80
#define SYSCALL_WIFI_CONTROL       81
#define SYSCALL_BT_CONTROL         82
#define SYSCALL_DND_SET            83
#define SYSCALL_DND_GET            84
#define SYSCALL_AIRPLANE_SET       85
#define SYSCALL_AIRPLANE_GET       86

/* ── Zircon IPC (SYSCALL_ZIRCON_IPC) sub-commands ──
 * a1 = 0: return shared ring physical address (or -1 if absent)
 * a1 = 1: recv — copy next message to user buffer (a2), 1/0/-1
 * a1 = 2: send — copy a message from user buffer (a2), 1/-1
 */
#define ZIRCON_IPC_QUERY  0
#define ZIRCON_IPC_RECV   1
#define ZIRCON_IPC_SEND   2

#define ZIRCON_IPC_MAGIC  0x5A495243  /* "ZIRC" */

typedef enum {
    ZIRCON_IPC_NONE = 0,
    ZIRCON_IPC_NOTIFY,        /* kernel → user: new notification */
    ZIRCON_IPC_APP_LAUNCH,    /* kernel → user: app launch request */
    ZIRCON_IPC_APP_CLOSE,     /* kernel → user: app close request */
    ZIRCON_IPC_TOUCH_EVENT,   /* kernel → user: touch/gesture event */
    ZIRCON_IPC_CMD_RESP,      /* user → kernel: command response */
    ZIRCON_IPC_APP_LIST,      /* user → kernel: available app list */
    ZIRCON_IPC_QS_TOGGLE,     /* user → kernel: toggle quick setting */
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

/* ── SYSCALL_FB_INFO payload (kernel `codeos_fb_info_t`) ── */
typedef struct {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint8_t  bpp;
    uint8_t  type;
} zircon_fb_info_t;

/* ── SYSCALL_INPUT_POLL payload (kernel `codeos_input_ev_t`) ──
 * ev.type: 0 = none, 1 = key pressed
 */
typedef struct {
    uint64_t reserved0;
    int      type;
    int      key;
    int      mouse_x;
    int      mouse_y;
    int      mouse_buttons;
    uint64_t reserved1;
} zircon_input_ev_t;

/* ── Userspace syscall helpers (implemented in stubs.c) ──
 * Thin typed wrappers over the native `int $0x80` ABI. */

/* Query the shared IPC page: returns its physical address, or -1 if the
 * kernel IPC ring is not available (no zircon_ipc_init() at boot). */
long zs_ipc_query(void);

/* Copy the next kernel→user message into *msg. Returns 1 (got), 0 (none),
 * or -1 (error). */
int zs_ipc_recv(zircon_ipc_msg_t *msg);

/* Send a user→kernel message. Returns 1 on success, -1 on error. */
int zs_ipc_send(const zircon_ipc_msg_t *msg);

/* Fill *info with the current framebuffer geometry. Returns 0 or -1. */
int zs_fb_info(zircon_fb_info_t *info);

/* Poll input. Fills *ev and returns ev.type (0 = none, 1 = key). */
int zs_input_poll(zircon_input_ev_t *ev);

#endif /* ZIRCON_ABI_H */