#ifndef ETEROS_NET_DNS_H
#define ETEROS_NET_DNS_H

#include <types.h>

/**
 * DNS Header Structure
 */
struct dns_header {
    uint16_t id;        /* Identification number */
    uint16_t flags;     /* DNS Flags */
    uint16_t q_count;   /* Number of Questions */
    uint16_t ans_count; /* Number of Answer RRs */
    uint16_t auth_count;/* Number of Authority RRs */
    uint16_t add_count; /* Number of Additional RRs */
} __attribute__((packed));

/**
 * Resolves a hostname to an IPv4 address using the configured DNS server.
 * Uses a simple UDP query on port 53.
 *
 * @param hostname The null-terminated hostname (e.g., "google.com")
 * @return The IPv4 address in little-endian format (0x01020304), or 0 on failure.
 */
uint32_t dns_resolve(const char* hostname);

#endif /* ETEROS_NET_DNS_H */
