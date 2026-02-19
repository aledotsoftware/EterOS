#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

/* Forward declaration for exit since -Iinclude shadows stdlib.h */
void exit(int status);

/* Mock Environment */
uint32_t my_ip = 0x0A000002; /* 10.0.0.2 */
uint8_t gateway_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
int e1000_called = 0;

/* Mocks */
uint16_t net_checksum(void* vdata, size_t length) {
    return 0; /* Dummy checksum */
}

int e1000_send_packet(const void* data, uint16_t len) {
    e1000_called = 1;
    printf("DEBUG: e1000_send_packet called with len %d\n", len);
    return 0; /* Success */
}

uint8_t* e1000_get_mac(void) {
    static uint8_t mac[] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    return mac;
}

uint64_t timer_get_ticks(void) { return 0; }
void task_yield(void) { }
void hal_console_write(const char* s) { printf("%s", s); }
void net_poll(void) { }

/* Include the source file under test to access static functions */
#include "../kernel/net/tcp.c"

void test_stack_overflow_protection(void) {
    printf("Running test_stack_overflow_protection...\n");

    socket_entry_t sock;
    memset(&sock, 0, sizeof(sock));
    sock.state = SOCKET_STATE_ESTABLISHED;
    sock.local_port = 12345;
    sock.remote_port = 80;
    sock.remote_ip = 0x08080808;

    /* Create payload larger than buffer size (1514) minus headers (~54) */
    char large_payload[2000];
    memset(large_payload, 'A', sizeof(large_payload));

    e1000_called = 0;

    /* Call tcp_send_packet directly.
       If vulnerable: it copies data (overflows stack) then calls e1000_send_packet.
       If fixed: it returns -1 immediately.
    */
    int result = tcp_send_packet(&sock, large_payload, sizeof(large_payload), TCP_PSH | TCP_ACK);

    if (e1000_called) {
        printf("FAIL: e1000_send_packet was called! Vulnerability present (stack overflow happened before this call).\n");
        exit(1);
    }

    if (result != -1) {
        printf("FAIL: tcp_send_packet returned %d, expected -1.\n", result);
        exit(1);
    }

    printf("PASS: Large packet rejected safely.\n");
}

int main(void) {
    test_stack_overflow_protection();
    return 0;
}
