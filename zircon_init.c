/* ───────────────────────── Zircon Init ─────────────────────────
 * PID 1 for the Zircon OS.  Boots the compositor and manages
 * app lifecycle.  Runs on the CodeOS kernel.
 * ───────────────────────────────────────────────────────────── */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "zircon_abi.h"

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

/* ── Signal handlers ── */

static void sig_term_handler(int sig) {
    (void)sig;
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

    /* Check kernel IPC bridge */
    if (zs_ipc_query() < 0) {
        printf("zircon: warning: IPC not available (running without kernel bridge)\n");
    } else {
        printf("zircon: IPC bridge connected\n");
    }

    /* Supervisor loop: launch the compositor, then block until it dies
     * (WAIT fully reaps the child), then relaunch it fresh. */
    for (;;) {
        launch_compositor();
        int status;
        pid_t pid = waitpid(-1, &status, 0);
        if (pid > 0 && pid == compositor_pid) {
            compositor_pid = 0;
            printf("zircon: compositor exited, restarting...\n");
        }
    }

    return 0;
}