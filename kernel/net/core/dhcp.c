
#include <types.h>
#include <net/dhcp.h>
int dhcp_parse_offer(const uint8_t* buffer, size_t len, uint32_t xid, const struct dhcp_packet** out_dhcp, size_t* out_dhcp_len) {
    (void)buffer; (void)len; (void)xid; (void)out_dhcp; (void)out_dhcp_len;
    return -1;
}
int dhcp_parse_options(const struct dhcp_packet* packet, size_t len, uint32_t* mask, uint32_t* gw, uint32_t* dns) {
    (void)packet; (void)len; (void)mask; (void)gw; (void)dns;
    return -1;
}
#include <types.h>



/**
 * éterOS - Simple DHCP Client
 * Implementación básica de DHCP Discover -> Offer.
 */

#include <net/dhcp.h>
#include <net/nic.h>
#include <string.h>
#include <vga.h>
#include <mm.h>
#include <timer.h>
#include <hal.h>

/* Constants and structs are now in include/net/dhcp.h and include/net/defs.h */


