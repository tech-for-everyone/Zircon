/* ─────────────────────────────────────────────────────────────
 * Zircon freestanding libc — real CodeOS syscall layer.
 *
 * All entry points are backed by the native `int $0x80` ABI
 * (RAX=number, RDI/RSI/RDX/R10/R8/R9 = a1..a6), see zircon_abi.h.
 * The FILE struct layout MUST stay { int _flags; unsigned char *_ptr; }
 * to match the freestd stdio.h used by the other TUs.
 * ───────────────────────────────────────────────────────────── */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <sys/types.h>
#include "zircon_abi.h"

/* ── FILE (matches freestd stdio.h layout) ── */
typedef struct _IO_FILE FILE;
struct _IO_FILE { int _flags; unsigned char *_ptr; };

FILE _stderr_obj = {0, 0};
FILE _stdout_obj = {1, 0};
FILE _stdin_obj  = {0, 0};
FILE *stderr = &_stderr_obj;
FILE *stdout = &_stdout_obj;
FILE *stdin  = &_stdin_obj;

/* ─────────────────────────────────────────────────────────────
 * Raw syscall wrappers (native ABI).
 * a4/a5/a6 are pinned to r10/r8/r9 like kernel/userspace unistd.h.
 * ───────────────────────────────────────────────────────────── */
static inline long syscall_0(long n) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n) : "memory");
    return ret;
}
static inline long syscall_1(long n, long a1) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1) : "memory");
    return ret;
}
static inline long syscall_2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "memory");
    return ret;
}
static inline long syscall_3(long n, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "memory");
    return ret;
}
static inline long syscall_4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long _a4 __asm__("r10") = a4;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(_a4) : "memory");
    return ret;
}
static inline long syscall_5(long n, long a1, long a2, long a3, long a4, long a5) {
    long ret;
    register long _a4 __asm__("r10") = a4;
    register long _a5 __asm__("r8")  = a5;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(_a4), "r"(_a5) : "memory");
    return ret;
}
static inline long syscall_6(long n, long a1, long a2, long a3, long a4, long a5, long a6) {
    long ret;
    register long _a4 __asm__("r10") = a4;
    register long _a5 __asm__("r8")  = a5;
    register long _a6 __asm__("r9")  = a6;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(_a4), "r"(_a5), "r"(_a6) : "memory");
    return ret;
}

/* ─────────────────────────────────────────────────────────────
 * Typed kernel helpers (zircon_abi.h) — non-static exports.
 * ───────────────────────────────────────────────────────────── */
long zs_ipc_query(void) {
    return syscall_1(SYSCALL_ZIRCON_IPC, ZIRCON_IPC_QUERY);
}
int zs_ipc_recv(zircon_ipc_msg_t *msg) {
    return (int)syscall_2(SYSCALL_ZIRCON_IPC, ZIRCON_IPC_RECV, (long)msg);
}
int zs_ipc_send(const zircon_ipc_msg_t *msg) {
    return (int)syscall_2(SYSCALL_ZIRCON_IPC, ZIRCON_IPC_SEND, (long)msg);
}
int zs_fb_info(zircon_fb_info_t *info) {
    return (int)syscall_1(SYSCALL_FB_INFO, (long)info);
}
int zs_input_poll(zircon_input_ev_t *ev) {
    return (int)syscall_1(SYSCALL_INPUT_POLL, (long)ev);
}

/* ─────────────────────────────────────────────────────────────
 * POSIX-ish I/O — thin over the kernel syscalls.
 * ───────────────────────────────────────────────────────────── */

/* Kernel WRITE ignores the fd (console write) and returns `count`. */
ssize_t write(int fd, const void *buf, size_t count) {
    (void)fd;
    return (ssize_t)syscall_2(SYSCALL_WRITE, (long)buf, (long)count);
}

ssize_t read(int fd, void *buf, size_t count) {
    return (ssize_t)syscall_3(SYSCALL_READ, (long)fd, (long)buf, (long)count);
}

int open(const char *path, int flags, ...) {
    return (int)syscall_2(SYSCALL_OPEN, (long)path, (long)flags);
}

int close(int fd) {
    return (int)syscall_1(SYSCALL_CLOSE, (long)fd);
}

off_t lseek(int fd, off_t offset, int whence) {
    return (off_t)syscall_3(SYSCALL_LSEEK, (long)fd, (long)offset, (long)whence);
}

/* ── Process control ── */

pid_t fork(void) {
    return (pid_t)syscall_0(SYSCALL_FORK);
}

/* Kernel EXECVE ABI: (path, argv, argc) — never returns on success. */
int execve(const char *path, char *const argv[], int argc) {
    return (int)syscall_3(SYSCALL_EXECVE, (long)path, (long)argv, (long)argc);
}

int execl(const char *path, const char *arg, ...) {
    char *argv[65];
    int argc = 0;
    if (arg) argv[argc++] = (char *)arg;
    va_list ap;
    va_start(ap, arg);
    const char *a;
    while (argc < 64 && (a = va_arg(ap, const char *)) != NULL)
        argv[argc++] = (char *)a;
    va_end(ap);
    argv[argc] = NULL;
    return execve(path, argv, argc);
}

/* The kernel WAIT always blocks and fully reaps; options ignored. */
int waitpid(pid_t pid, int *status, int options) {
    (void)options;
    return (int)syscall_2(SYSCALL_WAIT, (long)pid, (long)status);
}

int getpid(void)  { return (int)syscall_0(SYSCALL_GETPID); }
int getppid(void) { return (int)syscall_0(SYSCALL_GETPPID); }

int kill(pid_t pid, int sig) {
    return (int)syscall_2(SYSCALL_KILL, (long)pid, (long)sig);
}

/* ── Sleep / time ── */

unsigned int sleep(unsigned int seconds) {
    syscall_1(SYSCALL_SLEEP, (long)seconds * 1000);
    return 0;
}

int usleep(unsigned int usec) {
    long ms = (long)((usec + 999) / 1000);
    if (ms < 1) ms = 1;
    syscall_1(SYSCALL_SLEEP, ms);
    return 0;
}

time_t time(time_t *tloc) {
    time_t t = (time_t)syscall_0(SYSCALL_TIME);
    if (tloc) *tloc = t;
    return t;
}

/* ── Exit ── */

void _exit(int status) __attribute__((noreturn));
void _exit(int status) {
    syscall_1(SYSCALL_EXIT, (long)status);
    for (;;) { }
}

void exit(int status) __attribute__((noreturn));
void exit(int status) {
    _exit(status);
}

/* ── Signals (kernel delivers SIGTERM/SIGKILL via proc_kill) ── */

typedef void (*sighandler_t)(int);
#define SIG_DFL ((void (*)(int))0)
sighandler_t signal(int sig, sighandler_t handler) {
    (void)sig; (void)handler;
    return SIG_DFL;
}

/* ─────────────────────────────────────────────────────────────
 * vsnprintf — real minimal formatter.
 * Supports: %s %c %d %i %u %x %X %p %% %ld %lu %lx %g/%f (double)
 * plus '-' / '0' flags and width for left-padding.
 * ───────────────────────────────────────────────────────────── */

/* Emit one char: write only if room, but always count (vsnprintf semantics). */
static void zb_putc(char *out, size_t *pos, size_t max, char c) {
    if (*pos < max) out[*pos] = (char)c;
    (*pos)++;
}

static void fmt_int(char *out, size_t *pos, size_t max, unsigned long long v,
                    int base, int upper, int zero, int width, int left) {
    char tmp[32];
    int i = 0;
    static const char *digits_lo = "0123456789abcdef";
    static const char *digits_up = "0123456789ABCDEF";
    const char *dig = upper ? digits_up : digits_lo;
    if (v == 0) tmp[i++] = '0';
    while (v > 0) {
        tmp[i++] = dig[v % (unsigned)base];
        v /= (unsigned)base;
    }
    int len = i;
    int pad = width > len ? width - len : 0;
    if (!left && zero) {
        while (pad-- > 0) zb_putc(out, pos, max, '0');
    } else if (!left) {
        while (pad-- > 0) zb_putc(out, pos, max, ' ');
    }
    while (i > 0) zb_putc(out, pos, max, tmp[--i]);
    if (left) {
        while (pad-- > 0) zb_putc(out, pos, max, ' ');
    }
}

static void fmt_double(char *out, size_t *pos, size_t max, double d) {
    if (d < 0) { zb_putc(out, pos, max, '-'); d = -d; }
    unsigned long long ip = (unsigned long long)d;
    fmt_int(out, pos, max, ip, 10, 0, 0, 0, 0);
    double frac = d - (double)ip;
    if (frac != 0.0) {
        zb_putc(out, pos, max, '.');
        int wrote = 0;
        for (int k = 0; k < 6; k++) {
            frac *= 10.0;
            int digit = (int)frac;
            zb_putc(out, pos, max, (char)('0' + digit));
            frac -= (double)digit;
            wrote++;
            if (frac < 0.0000005) break;
        }
        /* trim trailing zeros of the fractional part */
        while (wrote > 0 && *pos > 0 && out[*pos - 1] == '0') {
            (*pos)--;
            wrote--;
        }
    }
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    size_t pos = 0;
    const size_t max = size > 0 ? size - 1 : 0; /* leave room for NUL */
    const char *p = fmt;

    while (*p) {
        if (*p != '%') {
            zb_putc(buf, &pos, max, *p);
            p++;
            continue;
        }
        p++;
        if (*p == '%') { zb_putc(buf, &pos, max, '%'); p++; continue; }
        int left = 0, zero = 0, width = 0;
        while (*p == '-' || *p == '0') {
            if (*p == '-') left = 1; else zero = 1;
            p++;
        }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        int is_long = 0;
        if (*p == 'l') { is_long = 1; p++; }

        char conv = *p;
        switch (conv) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            size_t len = 0;
            while (s[len]) len++;
            int pad = width > (int)len ? width - (int)len : 0;
            if (!left)
                while (pad-- > 0) zb_putc(buf, &pos, max, ' ');
            for (size_t k = 0; k < len; k++) zb_putc(buf, &pos, max, s[k]);
            if (left)
                while (pad-- > 0) zb_putc(buf, &pos, max, ' ');
            break;
        }
        case 'c': {
            int c = va_arg(ap, int);
            zb_putc(buf, &pos, max, (char)c);
            break;
        }
        case 'd':
        case 'i': {
            long long v = is_long ? (long long)va_arg(ap, long)
                                  : (long long)va_arg(ap, int);
            if (v < 0) {
                zb_putc(buf, &pos, max, '-');
                fmt_int(buf, &pos, max, (unsigned long long)(-v), 10, 0, zero, width > 1 ? width - 1 : 0, left);
            } else {
                fmt_int(buf, &pos, max, (unsigned long long)v, 10, 0, zero, width, left);
            }
            break;
        }
        case 'u':
            fmt_int(buf, &pos, max,
                    is_long ? (unsigned long long)va_arg(ap, unsigned long)
                            : (unsigned long long)va_arg(ap, unsigned int),
                    10, 0, zero, width, left);
            break;
        case 'x':
            fmt_int(buf, &pos, max,
                    is_long ? (unsigned long long)va_arg(ap, unsigned long)
                            : (unsigned long long)va_arg(ap, unsigned int),
                    16, 0, zero, width, left);
            break;
        case 'X':
            fmt_int(buf, &pos, max,
                    is_long ? (unsigned long long)va_arg(ap, unsigned long)
                            : (unsigned long long)va_arg(ap, unsigned int),
                    16, 1, zero, width, left);
            break;
        case 'p': {
            unsigned long long v = (unsigned long long)va_arg(ap, void *);
            zb_putc(buf, &pos, max, '0');
            zb_putc(buf, &pos, max, 'x');
            fmt_int(buf, &pos, max, v, 16, 0, 0, 0, 0);
            break;
        }
        case 'g':
        case 'f': {
            double d = va_arg(ap, double);
            fmt_double(buf, &pos, max, d);
            break;
        }
        default:
            zb_putc(buf, &pos, max, conv);
            break;
        }
        if (*p) p++;
    }
    if (size > 0) buf[pos < max ? pos : max] = '\0';
    return (int)pos;
}

/* sprintf must pass SIZE_MAX so vsnprintf never truncates. */
int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, (size_t)-1, fmt, ap);
    va_end(ap);
    return ret;
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

/* vfprintf — format into a stack buffer and emit with one write. */
int vfprintf(FILE *stream, const char *fmt, va_list ap) {
    (void)stream;
    char buf[1024];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n > 0) write(1, buf, (size_t)n);
    return n;
}

int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return ret;
}

int fprintf(FILE *stream, const char *fmt, ...) {
    (void)stream;
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return ret;
}

int puts(const char *s) {
    if (s) {
        size_t len = 0;
        while (s[len]) len++;
        write(1, s, len);
    }
    write(1, "\n", 1);
    return 0;
}

int fputs(const char *s, FILE *stream) {
    (void)stream;
    if (s) {
        size_t len = 0;
        while (s[len]) len++;
        write(1, s, len);
    }
    return 0;
}

void perror(const char *s) {
    if (s) {
        size_t len = 0;
        while (s[len]) len++;
        write(1, s, len);
    }
    write(1, ": error\n", 8);
}

int fputc(int c, FILE *stream) {
    (void)stream;
    write(1, &c, 1);
    return c;
}

int fflush(FILE *stream) { (void)stream; return 0; }

/* ─────────────────────────────────────────────────────────────
 * Heap — first-fit free list over sys_brk, grown page-at-a-time.
 * ───────────────────────────────────────────────────────────── */
#define ZB_MAGIC  0x5A42u
#define ZB_PAGESZ 4096u
#define ZB_ALIGN  16u

typedef struct zb_hdr {
    size_t size;          /* payload bytes (16-aligned) */
    struct zb_hdr *next;  /* free-list link when free */
    uint32_t magic;       /* ZB_MAGIC */
    uint32_t flags;       /* bit0 = in use */
    char _pad[8];         /* header totals 32 bytes */
} zb_hdr_t;

/* proc brk starts at PROC_BRK_BASE (0x60000000), grows to MAX. */
static uintptr_t heap_base = 0;   /* first brk value */
static uintptr_t heap_cur  = 0;   /* end of committed heap */
static zb_hdr_t *zb_free = NULL;

void *sys_brk(void *addr) {
    return (void *)syscall_1(SYSCALL_BRK, (long)addr);
}

/* Extend the heap by whole pages; returns new base of the region or NULL. */
static void *zb_grow(size_t need) {
    if (heap_base == 0) {
        heap_base = (uintptr_t)sys_brk((void *)0);
        if (heap_base == 0 || heap_base == (uintptr_t)-1) return NULL;
        heap_cur = heap_base;
    }
    size_t pages = (need + ZB_PAGESZ - 1) / ZB_PAGESZ;
    uintptr_t new_cur = heap_cur + pages * ZB_PAGESZ;
    /* BRK returns the old brk on failure — compare result. */
    void *r = sys_brk((void *)new_cur);
    if ((uintptr_t)r != new_cur && (uintptr_t)r != heap_cur)
        return NULL;
    if ((uintptr_t)r != new_cur)
        return NULL; /* failed to grow */
    void *base = (void *)heap_cur;
    heap_cur = new_cur;
    return base;
}

void *malloc(size_t size) {
    if (size == 0) size = 1;
    size = (size + (ZB_ALIGN - 1)) & ~(size_t)(ZB_ALIGN - 1);
    if (size < ZB_ALIGN) size = ZB_ALIGN;

    size_t need = size + sizeof(zb_hdr_t);

    /* First fit. */
    zb_hdr_t **pp = &zb_free;
    while (*pp) {
        zb_hdr_t *b = *pp;
        if (b->size >= size) {
            *pp = b->next;
            b->flags = 1;
            b->magic = ZB_MAGIC;
            /* Split off a remainder block if it is big enough to be useful. */
            size_t rem = b->size - size;
            if (rem >= sizeof(zb_hdr_t) + ZB_ALIGN) {
                zb_hdr_t *nb = (zb_hdr_t *)((char *)(b + 1) + size);
                nb->size = rem - sizeof(zb_hdr_t);
                nb->next = zb_free;
                nb->magic = ZB_MAGIC;
                nb->flags = 0;
                nb->_pad[0] = 0;
                zb_free = nb;
            }
            b->size = size;
            return (void *)(b + 1);
        }
        pp = &b->next;
    }

    /* Grow the heap; the fresh region becomes a free block. */
    zb_hdr_t *g = (zb_hdr_t *)zb_grow(need + sizeof(zb_hdr_t));
    if (!g) return NULL;
    size_t total = sizeof(zb_hdr_t) + size;
    size_t region = (total + ZB_PAGESZ - 1) & ~(size_t)(ZB_PAGESZ - 1);
    g->size = size;
    g->magic = ZB_MAGIC;
    g->flags = 1;
    g->next = NULL;
    g->_pad[0] = 0;
    size_t leftover = region - total;
    if (leftover >= sizeof(zb_hdr_t) + ZB_ALIGN) {
        zb_hdr_t *nb = (zb_hdr_t *)((char *)(g + 1) + size);
        nb->size = leftover - sizeof(zb_hdr_t);
        nb->next = zb_free;
        nb->magic = ZB_MAGIC;
        nb->flags = 0;
        nb->_pad[0] = 0;
        zb_free = nb;
    }
    return (void *)(g + 1);
}

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total ? total : 1);
    if (p) {
        unsigned char *q = p;
        for (size_t i = 0; i < total; i++) q[i] = 0;
    }
    return p;
}

void free(void *ptr) {
    if (!ptr) return;
    zb_hdr_t *b = (zb_hdr_t *)ptr - 1;
    if (b->magic != ZB_MAGIC) return;
    b->flags = 0;
    b->next = zb_free;
    zb_free = b;
}

void *realloc(void *ptr, size_t size) {
    if (!ptr) return malloc(size);
    zb_hdr_t *b = (zb_hdr_t *)ptr - 1;
    size_t old = b->size;
    if (old >= size) return ptr;
    void *np = malloc(size);
    if (!np) return NULL;
    unsigned char *d = np, *s = ptr;
    for (size_t i = 0; i < old; i++) d[i] = s[i];
    free(ptr);
    return np;
}

/* ─────────────────────────────────────────────────────────────
 * String helpers.
 * NOTE: memcmp+fma come from browser/net_shim.c — do not redefine.
 * ───────────────────────────────────────────────────────────── */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    while (n-- && *s1 && (*s1 == *s2)) { s1++; s2++; }
    if (n == (size_t)-1) return 0;
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    while (i < n) dest[i++] = '\0';
    return dest;
}

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++)) {}
    return dest;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (char *)0;
}

int atoi(const char *s) {
    int v = 0, neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return neg ? -v : v;
}

/* ── ctime — real civil-from-epoch formatting ── */
/* Days since 1970-01-01 → y/m/d, Howard Hinnant style. */
static void zb_civil_from_days(long z, long *y, long *m, long *d) {
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long yy = (long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    unsigned dd = doy - (153 * mp + 2) / 5 + 1;
    unsigned mm = mp < 10 ? mp + 3 : mp - 9;
    yy += (mm <= 2);
    *y = yy; *m = mm; *d = dd;
}

char *ctime(const time_t *t) {
    static char buf[32];
    static const char *wd[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *mo[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                               "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    long s = t ? (long)*t : 0;
    long days = s / 86400;
    long rem = s % 86400;
    if (rem < 0) { rem += 86400; days--; }
    long wday = ((days % 7) + 4) % 7; /* 1970-01-01 = Thursday */
    if (wday < 0) wday += 7;
    long yy, mm, dd;
    zb_civil_from_days(days, &yy, &mm, &dd);
    long hh = rem / 3600, mi = (rem % 3600) / 60, ss = rem % 60;
    sprintf(buf, "%s %s %2ld %02ld:%02ld:%02ld %ld\n",
            wd[wday], mo[mm - 1], dd, hh, mi, ss, yy);
    return buf;
}

/* ── Stream FILE I/O (minimal, fd-backed) ── */

/* FREESTD header layout: { int _flags; unsigned char *_ptr; }.
 * We store the fd in _flags for streams returned by fopen. */
#define ZB_NFILES 16
static FILE zb_files[ZB_NFILES];
static int zb_file_used[ZB_NFILES];
#define ZB_FD(stream) ((stream) ? ((stream)->_flags > 2 ? (stream)->_flags : 1) : 1)

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    if (total == 0) return 0;
    ssize_t n = write(ZB_FD(stream), ptr, total);
    return n > 0 ? (size_t)n / size : 0;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t total = size * nmemb;
    if (total == 0) return 0;
    ssize_t n = read(ZB_FD(stream), ptr, total);
    return n > 0 ? (size_t)n / size : 0;
}

FILE *fopen(const char *path, const char *mode) {
    int flags = 0;
    if (mode) {
        if (mode[0] == 'r')      flags = 0;
        else if (mode[0] == 'w') flags = 1;
        else if (mode[0] == 'a') flags = 1;
        if (mode[0] == 'r' && (mode[1] == '+' || (mode[1] && mode[2] == '+')))
            flags = 2;
    }
    int fd = open(path, flags);
    if (fd < 0) return (FILE *)0;
    FILE *f = (FILE *)0;
    for (int i = 0; i < ZB_NFILES; i++) {
        if (!zb_file_used[i]) {
            zb_file_used[i] = 1;
            zb_files[i]._flags = fd;
            zb_files[i]._ptr = (unsigned char *)0;
            f = &zb_files[i];
            break;
        }
    }
    if (!f) close(fd);
    return f;
}

int fclose(FILE *stream) {
    if (!stream) return 0;
    if (stream->_flags > 2) {
        close(stream->_flags);
        for (int i = 0; i < ZB_NFILES; i++)
            if (&zb_files[i] == stream) zb_file_used[i] = 0;
    }
    stream->_flags = 0;
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    off_t r = lseek(ZB_FD(stream), offset, whence);
    return r < 0 ? -1 : 0;
}

long ftell(FILE *stream) {
    off_t r = lseek(ZB_FD(stream), 0, 1);
    return (long)r;
}

int fgetc(FILE *stream) {
    unsigned char c;
    ssize_t n = read(ZB_FD(stream), &c, 1);
    return n == 1 ? (int)c : -1;
}

/* ── Generic variadic syscall (compat) ── */
long syscall(long number, ...) {
    va_list ap;
    va_start(ap, number);
    long a1 = va_arg(ap, long);
    long a2 = va_arg(ap, long);
    long a3 = va_arg(ap, long);
    long a4 = va_arg(ap, long);
    long a5 = va_arg(ap, long);
    long a6 = va_arg(ap, long);
    va_end(ap);
    return syscall_6(number, a1, a2, a3, a4, a5, a6);
}