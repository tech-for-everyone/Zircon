/* Minimal appvm stubs for Zircon */
#ifndef APPVM_H
#define APPVM_H

#include <stdint.h>

static inline int appvm_init(void) { return 0; }
static inline int appvm_run(const char *cmd) { (void)cmd; return 0; }
static inline int appvm_exec(const char *path, char *const argv[]) { (void)path; (void)argv; return 0; }

#endif
