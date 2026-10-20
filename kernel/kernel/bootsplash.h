/* Minimal bootsplash stubs for Zircon */
#ifndef BOOTSPLASH_H
#define BOOTSPLASH_H

#include <stdint.h>

static inline void bootsplash_init(void) { }
static inline void bootsplash_finish(void) { }
static inline void bootsplash_set_progress(int pct, const char *msg) { (void)pct; (void)msg; }

#endif
