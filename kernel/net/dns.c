/**
 * éterOS - Simple DNS Client (UDP)
 * Resolves A records using Raw UDP packets.
 */

#include "../../include/net/dns.h"
#include "../../include/net/defs.h"
#include "../../include/net/e1000.h"
#include "../../include/string.h"
#include "../../include/timer.h"
#include "../../include/vga.h"
#include "../../include/mm.h"
#include "../../include/task.h"

extern uint32_t my_ip;
extern uint32_t gateway_ip;
extern uint32_t dns_ip;
extern uint8_t  gateway_mac[6];
extern int      net_arp_lookup(uint32_t target_ip);
extern uint16_t net_checksum(void* vdata, size_t length);

#define DNS_PORT 53
#define DNS_QUERY_TIMEOUT 3000 /* 3 seconds */

/* Helper to convert "google.com" -> "\x06google\x03com\x00" */
/* Returns length of encoded name */
static int dns_encode_name(uint8_t* buffer, const char* hostname) {
    int len = strlen(hostname);
    int pos = 0;
    int last_dot = -1;

    for (int i = 0; i < len; i++) {
        if (hostname[i] == '.') {
            int label_len = i - last_dot - 1;
            buffer[pos++] = (uint8_t)label_len;
            memcpy(&buffer[pos], &hostname[last_dot + 1], label_len);
            pos += label_len;
            last_dot = i;
        }
    }

    /* Last label */
    int label_len = len - last_dot - 1;
    if (label_len > 0) {
        buffer[pos++] = (uint8_t)label_len;
        memcpy(&buffer[pos], &hostname[last_dot + 1], label_len);
        pos += label_len;
    }

    buffer[pos++] = 0; /* Null terminator */
    return pos;
}

uint32_t dns_resolve(const char* hostname) {
    if (dns_ip == 0) {
        /* Default to Google DNS if not set */
        dns_ip = 0x08080808; 
    }
    
    /* Ensure Gateway MAC is known */
    if (gateway_mac[0] == 0xFF) {
        if (net_arp_lookup(gateway_ip) != 0) return 0;
    }

    /* Verify local IP */
    if (my_ip == 0) return 0;

    uint8_t buffer[1024];
    memset(buffer, 0, sizeof(buffer));

    struct ethernet_header* eth = (struct ethernet_header*)buffer;
    struct ip_header* ip = (struct ip_header*)(buffer + sizeof(struct ethernet_header));
    struct udp_header* udp = (struct udp_header*)(buffer + sizeof(struct ethernet_header) + sizeof(struct ip_header));
    uint8_t* dns_payload = buffer + sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct udp_header);

    /* 1. Build DNS Query */
    struct dns_header* dns = (struct dns_header*)dns_payload;
    uint16_t tx_id = (timer_get_ticks() & 0xFFFF) ^ 0xCAFE;
    dns->id = htons(tx_id);
    dns->flags = htons(0x0100); /* Standard Query, Recursion Desired */
    dns->q_count = htons(1);
    
    int query_len = sizeof(struct dns_header);
    uint8_t* qname = dns_payload + query_len;
    int name_len = dns_encode_name(qname, hostname);
    query_len += name_len;

    /* QType (A = 1) and QClass (IN = 1) */
    uint16_t* qtype = (uint16_t*)(dns_payload + query_len);
    *qtype = htons(1);
    query_len += 2;
    uint16_t* qclass = (uint16_t*)(dns_payload + query_len);
    *qclass = htons(1);
    query_len += 2;

    /* 2. Build UDP Header */
    udp->src_port = htons(54321 + (timer_get_ticks() % 1000));
    udp->dest_port = htons(DNS_PORT);
    udp->len = htons(sizeof(struct udp_header) + query_len);
    udp->checksum = 0;

    /* 3. Build IP Header */
    ip->ver_ihl = 0x45;
    ip->len = htons(sizeof(struct ip_header) + sizeof(struct udp_header) + query_len);
    ip->ttl = 64;
    ip->proto = IP_PROTO_UDP;
    ip->src = my_ip;
    ip->dest = dns_ip;
    ip->checksum = 0;
    ip->checksum = net_checksum(ip, sizeof(struct ip_header));

    /* 4. Build Ethernet Header */
    memcpy(eth->dest, gateway_mac, 6);
    memcpy(eth->src, e1000_get_mac(), 6);
    eth->type = htons(ETHERNET_TYPE_IP);

    /* Send */
    int packet_len = sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct udp_header) + query_len;
    e1000_send_packet(buffer, packet_len);

    /* 5. Wait for Response */
    uint64_t start = timer_get_ticks();
    uint8_t rx[1514];

    while (timer_get_ticks() - start < DNS_QUERY_TIMEOUT) {
        int rlen = e1000_receive(rx, sizeof(rx));
        if (rlen > 0) {
            struct ip_header* rip = (struct ip_header*)(rx + 14);
            if (rip->proto == IP_PROTO_UDP && rip->src == dns_ip) {
                /* Skip IP header length */
                 int ip_hlen = (rip->ver_ihl & 0xF) * 4;
                 struct udp_header* rudp = (struct udp_header*)((uint8_t*)rip + ip_hlen);
                 
                 /* Check Ports */
                 if (rudp->src_port == htons(DNS_PORT)) {
                     /* Parsing DNS Response */
                     uint8_t* rdns_payload = (uint8_t*)rudp + sizeof(struct udp_header);
                     struct dns_header* rdns = (struct dns_header*)rdns_payload;
                     
                     if (rdns->id == htons(tx_id)) {
                         /* Found it! Parse Answers. */
                         int ans_count = ntohs(rdns->ans_count);
                         if (ans_count > 0) {
                             /* Skip Questions to find Answers */
                             int q_count = ntohs(rdns->q_count);
                             uint8_t* ptr = rdns_payload + sizeof(struct dns_header);
                             
                             /* Skip Queries */
                             for (int i=0; i<q_count; i++) {
                                 while (*ptr != 0) {
                                     if (*ptr >= 192) { ptr+=2; goto end_name; } /* Compression ptr */
                                     ptr += (*ptr) + 1;
                                 }
                                 ptr++; /* Null */
                                 end_name:
                                 ptr += 4; /* Type + Class */
                             }

                             /* Parse Answers */
                             for (int i=0; i<ans_count; i++) {
                                 /* Name */
                                 if (*ptr >= 192) {
                                     ptr += 2; /* Pointer */
                                 } else {
                                     while (*ptr != 0) ptr += (*ptr) + 1;
                                     ptr++;
                                 }

                                 uint16_t type = ntohs(*(uint16_t*)ptr); ptr+=2;
                                 uint16_t class = ntohs(*(uint16_t*)ptr); ptr+=2;
                                 uint32_t ttl = ntohl(*(uint32_t*)ptr); ptr+=4; (void)ttl;
                                 uint16_t dlen = ntohs(*(uint16_t*)ptr); ptr+=2;

                                 if (type == 1 && class == 1 && dlen == 4) {
                                     /* Found A Record! */
                                     uint32_t ip_addr;
                                     memcpy(&ip_addr, ptr, 4);
                                     return ip_addr;
                                 }
                                 ptr += dlen;
                             }
                         }
                     }
                 }
            }
        }
        
        /* Non-blocking yield */
        task_yield();
        /* Keep net_poll active? Dhcp.c does explicit e1000_receive loop like here. 
           But ideally we should net_poll() too to clear buffer? 
           Here we consume packets directly. */
    }

    return 0; /* Failure */
}
