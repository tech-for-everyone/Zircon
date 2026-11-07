/* Network stubs for builds that intentionally omit the Zircon network stack.
 *
 * The kernel Makefile excludes this file when the network stack is enabled.
 *
 * That stayed invisible while the syscall dispatcher was dead code: nothing
 * referenced syscall_entry, --gc-sections dropped the whole dispatcher, and
 * with it the references to these functions.  Turning the SYSCALL MSRs on --
 * which every userspace program needs, since the `syscall` instruction is
 * how they reach the kernel at all -- made the dispatcher live, and the
 * missing symbols turned into link errors.
 *
 * These functions are no-ops that fail, preserving linkable network-disabled
 * builds without returning success-shaped results.
 *
 * NOTE: net.h is deliberately NOT included here.  The Zircon copy of net.h
 * declares net_get_ip() as a `static inline`, so including it in this file
 * would clash with the external definition below.  The declarations come
 * from the real headers instead.
 */
#include <stdint.h>

int tcp_socket(void) { return -1; }

int tcp_connect(uint32_t ip, uint16_t port) {
    (void)ip; (void)port;
    return -1;
}

int tcp_bind(int fd, uint32_t ip, uint16_t port) {
    (void)fd; (void)ip; (void)port;
    return -1;
}

int tcp_recv(int fd, void *buf, int len) {
    (void)fd; (void)buf; (void)len;
    return -1;
}

int tcp_close(int fd) {
    (void)fd;
    return -1;
}

int ip_send(uint32_t dst, uint8_t proto, const void *data, uint16_t len) {
    (void)dst; (void)proto; (void)data; (void)len;
    return -1;
}

uint32_t net_get_ip(void) { return 0; }

int tcp_listen(uint32_t ip, uint16_t port) {
    (void)ip; (void)port;
    return -1;
}

int tcp_accept(int fd, uint32_t *ip, uint16_t *port) {
    (void)fd; (void)ip; (void)port;
    return -1;
}

int tcp_send(int fd, const void *buf, int len) {
    (void)fd; (void)buf; (void)len;
    return -1;
}

int tcp_get_peer(int fd, uint32_t *ip, uint16_t *port) {
    (void)fd; (void)ip; (void)port;
    return -1;
}

/* HTTP transport expected by optional kernel services in network-disabled
 * builds. */
int http_post(const char *host, uint16_t port, const char *path,
              const void *body, uint16_t body_len,
              void *buf, uint16_t max_len) {
    (void)host; (void)port; (void)path;
    (void)body; (void)body_len; (void)buf; (void)max_len;
    return -1;
}
