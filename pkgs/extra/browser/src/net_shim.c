/* net_shim.c — freestanding network + math shims for the OpenWeb Rust lib.
 *
 * libow_http.a (ow_http.rs) references http_get/http_post/https_get/fs_read
 * and sched_sleep_ms.  Zircon on QEMU has no routed NIC for userspace yet, so
 * these report "network unavailable"; the browser still starts tabs and the
 * Rust renderer fills the C globals when a fetch succeeds (e.g. later wiring).
 * Also supplies memcmp/fma that Rust codegen needs.
 */
#include <stdint.h>
#include <stddef.h>

/* ── Network (declared in pkgs/core/panels/src/net.h) ── */

int http_get(const char *host, uint16_t port, const char *path,
             void *buf, uint16_t max_len) {
    (void)host; (void)port; (void)path; (void)buf; (void)max_len;
    return -1;
}

int http_post(const char *host, uint16_t port, const char *path,
              const void *body, uint16_t body_len,
              void *buf, uint16_t max_len) {
    (void)host; (void)port; (void)path; (void)body; (void)body_len;
    (void)buf; (void)max_len;
    return -1;
}

int https_get(const char *host, uint16_t port, const char *path,
              void *buf, uint16_t max_len) {
    (void)host; (void)port; (void)path; (void)buf; (void)max_len;
    return -1;
}

int fs_read(void *buf, int len) {
    (void)buf; (void)len;
    return -1;
}

/* ── Scheduler/thread sleep used by the Rust tab worker ── */

void sched_sleep_ms(int ms) {
    /* Busy-wait; ~1GHz guest. Good enough for pacing polls. */
    volatile uint64_t loops = (uint64_t)ms * 1000000UL / 8UL;
    while (loops--) { __asm__ volatile("" ::: "memory"); }
}

/* ── Compiler-rt / libm symbols Rust may need ── */

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char *)a;
    const unsigned char *y = (const unsigned char *)b;
    for (size_t i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    }
    return 0;
}

double fma(double a, double b, double c) {
    return a * b + c;
}