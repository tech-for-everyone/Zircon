/* Dummy net.h for Zircon (no network stack) */
#ifndef NET_H
#define NET_H

#include <stdint.h>

/* Dummy network functions */
static inline int net_init(void) { return 0; }
static inline int net_ready(void) { return 0; }
static inline uint32_t net_get_ip(void) { return 0; }
static inline uint32_t net_get_gateway(void) { return 0; }
static inline uint32_t net_get_dns(void) { return 0; }
static inline void net_poll(void) { }
static inline int dhcp_configure(void) { return -1; }

#endif
