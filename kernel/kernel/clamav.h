/* Dummy clamav.h for Zircon (no ClamAV integration) */
#ifndef CLAMAV_H
#define CLAMAV_H

#include <stdint.h>

/* Dummy ClamAV functions */
static inline int clamav_init(void) { return 0; }
static inline int clamav_scan(const void* data, int len) { (void)data; (void)len; return 0; }

#endif
