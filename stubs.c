/* Zircon freestanding stubs for standard C library functions */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

/* Forward declarations */
typedef struct _IO_FILE FILE;
struct _IO_FILE { int _flags; unsigned char *_ptr; };

extern FILE *stderr;
extern FILE *stdout;
extern FILE *stdin;

FILE _stderr_obj = {0, 0};
FILE _stdout_obj = {0, 0};
FILE _stdin_obj = {0, 0};
FILE *stderr = &_stderr_obj;
FILE *stdout = &_stdout_obj;
FILE *stdin = &_stdin_obj;

/* write - write to file descriptor */
ssize_t write(int fd, const void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return count;
}

/* read - read from file descriptor */
ssize_t read(int fd, void *buf, size_t count) {
    (void)fd; (void)buf; (void)count;
    return 0;
}

/* open - open file */
int open(const char *path, int flags, ...) {
    (void)path; (void)flags;
    return 1;
}

/* close - close file */
int close(int fd) {
    (void)fd;
    return 0;
}

/* lseek - seek file */
off_t lseek(int fd, off_t offset, int whence) {
    (void)fd; (void)offset; (void)whence;
    return 0;
}

/* vfprintf - variable argument formatted output */
int vfprintf(FILE *stream, const char *fmt, va_list ap);

/* printf - formatted output */
int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return ret;
}

/* fprintf - formatted output to stream */
int fprintf(FILE *stream, const char *fmt, ...) {
    (void)stream;
    va_list ap;
    va_start(ap, fmt);
    int ret = vfprintf(stdout, fmt, ap);
    va_end(ap);
    return ret;
}

/* vfprintf - variable argument formatted output */
int vfprintf(FILE *stream, const char *fmt, va_list ap) {
    (void)stream;
    const char *p = fmt;
    while (*p) {
        if (*p == '%') {
            p++;
            switch (*p) {
                case 's': {
                    char *s = va_arg(ap, char *);
                    if (s) while (*s) { write(1, s, 1); s++; }
                    break;
                }
                case 'd': {
                    int v = va_arg(ap, int);
                    if (v < 0) { write(1, "-", 1); v = -v; }
                    if (v == 0) { write(1, "0", 1); }
                    else {
                        char buf[20];
                        int i = 0;
                        while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
                        while (i > 0) write(1, &buf[--i], 1);
                    }
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(ap, int);
                    write(1, &c, 1);
                    break;
                }
                case '%':
                    write(1, "%", 1);
                    break;
                default:
                    write(1, p, 1);
                    break;
            }
        } else {
            write(1, p, 1);
        }
        p++;
    }
    return 0;
}

/* vsnprintf - variable argument formatted output to string */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

/* execl - execute program */
int execl(const char *path, const char *arg, ...) {
    (void)path; (void)arg;
    return -1;
}

/* perror - print error message */
int perror(const char *s) {
    (void)s;
    write(1, "error: ", 7);
    return 0;
}

/* _exit - terminate process */
void _exit(int status) {
    (void)status;
    __builtin_trap();
}

/* fork - create process */
pid_t fork(void) {
    return -1;
}

/* waitpid - wait for process */
int waitpid(pid_t pid, int *status, int options) {
    (void)pid; (void)status; (void)options;
    return -1;
}

/* kill - send signal */
int kill(pid_t pid, int sig) {
    (void)pid; (void)sig;
    return 0;
}

/* signal - set signal handler */
typedef void (*sighandler_t)(int);
#define SIG_DFL ((void (*)(int))0)
sighandler_t signal(int sig, sighandler_t handler) {
    (void)sig; (void)handler;
    return SIG_DFL;
}

/* puts - output string with newline */
int puts(const char *s) {
    if (s) {
        size_t len = 0;
        while (s[len]) len++;
        write(1, s, len);
        write(1, "\n", 1);
    }
    return 0;
}

/* fputs - output string to stream */
int fputs(const char *s, FILE *stream) {
    (void)stream;
    if (s) {
        size_t len = 0;
        while (s[len]) len++;
        write(1, s, len);
    }
    return 0;
}

/* sprintf - formatted output to string */
int sprintf(char *buf, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, 0, fmt, ap);
    va_end(ap);
    return ret;
}

/* snprintf - formatted output to string with size limit */
int snprintf(char *buf, size_t size, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

/* vsnprintf - variable argument formatted output to string */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap) {
    (void)buf; (void)size; (void)fmt; (void)ap;
    return 0;
}

/* strcmp - compare strings */
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) { s1++; s2++; }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* memcpy - copy memory */
void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    while (n--) *d++ = *s++;
    return dest;
}

/* memmove - move memory */
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

/* memset - set memory */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

/* strlen - string length */
size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

/* strncpy - copy string with limit */
char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i]; i++) dest[i] = src[i];
    while (i < n) dest[i++] = '\0';
    return dest;
}

/* strchr - find character in string */
char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return 0;
}

/* atoi - string to integer */
int atoi(const char *s) {
    int v = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}

/* exit - terminate program */
void exit(int status) {
    (void)status;
    __builtin_trap();
}

/* syscall - generic syscall wrapper */
long syscall(long number, ...) {
    (void)number;
    va_list ap;
    va_start(ap, number);
    va_end(ap);
    return -1;
}

/* sys_brk - break/heap management */
void *sys_brk(void *addr) {
    (void)addr;
    return (void *)-1;
}

/* malloc - allocate memory */
void *malloc(size_t size) {
    (void)size;
    return 0;
}

/* free - free memory */
void free(void *ptr) {
    (void)ptr;
}

/* calloc - allocate zeroed memory */
void *calloc(size_t nmemb, size_t size) {
    (void)nmemb; (void)size;
    return 0;
}

/* fwrite - write to stream */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr; (void)size; (void)nmemb; (void)stream;
    return nmemb;
}

/* fread - read from stream */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    (void)ptr; (void)size; (void)nmemb; (void)stream;
    return 0;
}

/* fopen - open file stream */
FILE *fopen(const char *path, const char *mode) {
    (void)path; (void)mode;
    return 0;
}

/* fclose - close file stream */
int fclose(FILE *stream) {
    (void)stream;
    return 0;
}

/* fseek - seek stream */
int fseek(FILE *stream, long offset, int whence) {
    (void)stream; (void)offset; (void)whence;
    return 0;
}

/* ftell - tell position */
long ftell(FILE *stream) {
    (void)stream;
    return 0;
}

/* fgetc - get character from stream */
int fgetc(FILE *stream) {
    (void)stream;
    return -1;
}

/* fputc - put character to stream */
int fputc(int c, FILE *stream) {
    (void)stream;
    write(1, &c, 1);
    return c;
}

/* fflush - flush stream */
int fflush(FILE *stream) {
    (void)stream;
    return 0;
}

/* sleep */
unsigned int sleep(unsigned int seconds) {
    (void)seconds;
    return 0;
}

/* usleep */
int usleep(unsigned int usec) {
    (void)usec;
    return 0;
}
