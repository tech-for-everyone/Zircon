/* ───────────────────────── Zircon Init ─────────────────────────
 * PID 1 for the Zircon OS.  Boots the compositor and manages
 * app lifecycle.  Runs on the CodeOS kernel.
 * ───────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

/* ── Zircon IPC ── */

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
    long phys = syscall(7, 0, 0, 0, 0, 0);
    if (phys <= 0) return -1;
    int fd = open("/dev/mem", O_RDWR);
    if (fd < 0) return -1;
    ipc_ring = (zircon_ipc_ring_t *)mmap(0, 4096, PROT_READ|PROT_WRITE,
                                          MAP_SHARED, fd, phys);
    close(fd);
    return (ipc_ring && ipc_ring->magic == ZIRCON_IPC_MAGIC) ? 0 : -1;
}

/* ── Process management ── */

static pid_t compositor_pid = 0;

static void launch_compositor(void) {
    pid_t pid = fork();
    if (pid == 0) {
        /* Child: exec zircond */
        execl("/sbin/zircond", "zircond", NULL);
        perror("zircon: failed to exec zircond");
        _exit(127);
    } else if (pid > 0) {
        compositor_pid = pid;
        printf("zircon: compositor launched (PID %d)\n", pid);
    } else {
        perror("zircon: fork failed");
    }
}

static void reap_children(void) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (pid == compositor_pid) {
            compositor_pid = 0;
            printf("zircon: compositor exited, restarting...\n");
            launch_compositor();
        }
    }
}

/* ── Signal handlers ── */

static void sig_term_handler(int sig) {
    printf("zircon: received SIGTERM, shutting down...\n");
    if (compositor_pid > 0) kill(compositor_pid, SIGTERM);
    _exit(0);
}

static void sig_int_handler(int sig) {
    sig_term_handler(sig);
}

/* ── Main ── */

int main(void) {
    printf("╔══════════════════════════════════════════╗\n");
    printf("║         Zircon OS v1.0.0                 ║\n");
    printf("║   Google-free mobile/desktop OS          ║\n");
    printf("╚══════════════════════════════════════════╝\n");

    /* Set up signal handlers */
    signal(SIGTERM, sig_term_handler);
    signal(SIGINT, sig_int_handler);
    signal(SIGHUP, sig_term_handler);

    /* Initialize IPC with kernel */
    if (zircon_ipc_init() != 0) {
        printf("zircon: warning: IPC not available (running without kernel bridge)\n");
    } else {
        printf("zircon: IPC bridge connected\n");
    }

    /* Launch the compositor (PID 1 manages it) */
    launch_compositor();

    /* Main loop: reap children and keep the OS alive */
    while (1) {
        reap_children();
        if (compositor_pid <= 0) {
            printf("zircon: no compositor running, launching...\n");
            launch_compositor();
        }
        sleep(1);
    }

    return 0;
}
