#ifndef ZIRCON_MOVEON_SYSCALLS_H
#define ZIRCON_MOVEON_SYSCALLS_H

#include <codeos/syscall_abi.h>

#define O_RDONLY 0
#define O_WRONLY 1
#define O_CREAT  0x40

typedef struct {
    int size;
    int is_dir;
} stat_t;

static inline int sys_open(const char *path, int flags) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_OPEN), "D"(path), "S"(flags) : "memory");
    return ret;
}

static inline int sys_read(int fd, void *buf, int count) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_READ), "D"(fd), "S"(buf), "d"(count) : "memory");
    return ret;
}

static inline int sys_close(int fd) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_CLOSE), "D"(fd));
    return ret;
}

static inline int sys_pwrite(int fd, const void *buf, int count) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_PWRITE), "D"(fd), "S"(buf), "d"(count) : "memory");
    return ret;
}

static inline int sys_stat(const char *path, stat_t *buf) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_STAT), "D"(path), "S"(buf) : "memory");
    return ret;
}

static inline int sys_mkdir(const char *path) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_MKDIR), "D"(path) : "memory");
    return ret;
}

static inline int sys_readdir(const char *path, char *names, int max_bytes) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(SYSCALL_READDIR), "D"(path), "S"(names), "d"(max_bytes) : "memory");
    return ret;
}

#endif
