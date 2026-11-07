#ifndef NET_H
#define NET_H

#include <stdint.h>

struct eth_hdr {
    uint8_t dst_mac[6];
    uint8_t src_mac[6];
    uint16_t type;
} __attribute__((packed));

struct arp_hdr {
    uint16_t htype;
    uint16_t ptype;
    uint8_t hlen;
    uint8_t plen;
    uint16_t oper;
    uint8_t sha[6];
    uint32_t spa;
    uint8_t tha[6];
    uint32_t tpa;
} __attribute__((packed));

struct ip_hdr {
    uint8_t ver_ihl;
    uint8_t tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed));

struct tcp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t offset;
    uint8_t flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} __attribute__((packed));

struct udp_hdr {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t len;
    uint16_t checksum;
} __attribute__((packed));

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

typedef void (*http_progress_cb)(int received, int total);

int net_init(void);
int net_ready(void);
uint32_t net_get_ip(void);
uint32_t net_get_gateway(void);
uint32_t net_get_dns(void);
void net_get_mac(uint8_t *mac);
void net_poll(void);
int dhcp_configure(void);
void net_set_ip(uint32_t ip);
void net_set_gateway(uint32_t ip);
void net_set_dns(uint32_t ip);

int http_get(const char *host, uint16_t port, const char *path,
             void *buf, uint16_t max_len);
int https_get(const char *host, uint16_t port, const char *path,
              void *buf, uint16_t max_len);

#endif
