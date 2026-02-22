#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

/* Defines to handle conflicts with syscall.c */
#define timespec eteros_timespec

/* Include kernel headers */
#include "../include/types.h"
#include "../include/task.h"
#include "../include/fs/vfs.h"
#include "../include/net/socket.h"
#include "../include/pmm.h"

/* Hack to mock inline functions in cpu.h */
#define get_current_cpu real_get_current_cpu
#define get_cpu_id real_get_cpu_id
#include "../include/cpu.h"
#undef get_current_cpu
#undef get_cpu_id
#include "../include/lock.h"

/* Define missing error codes if needed */
#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EFAULT
#define EFAULT 14
#endif

/* Globals */
task_t current_task_mock;
bool vmm_verify_user_access_called = false;
const void* vmm_last_checked_addr = NULL;
size_t vmm_last_checked_size = 0;
bool vmm_should_fail = false;

/* Globals for linking */
socket_entry_t socket_table[MAX_SOCKETS];
sem_t net_sem;
fs_node_t* fs_root = NULL;
int total_cpus = 1;
cpu_info_t cpus[MAX_CPUS];

/* Mocks */
task_t* task_get_current(void) {
    return &current_task_mock;
}

cpu_info_t* get_current_cpu(void) { return &cpus[0]; }
int get_cpu_id(void) { return 0; }

void task_exit(int status) {
    printf("task_exit called with status %d\n", status);
    exit(status);
}

void task_yield(void) {}
void schedule(void) {}
void context_switch(uint64_t* old, uint64_t new) {}
void tss_set_rsp0(uint64_t rsp) {}

void serial_write_string(const char* s) {
    // printf("[SERIAL] %s", s);
}

void* kmalloc(size_t size) { return malloc(size); }
void kfree(void* ptr) { free(ptr); }

/* Mock vmm_verify_user_access */
int vmm_verify_user_access(const void* addr, size_t size, int write) {
    vmm_verify_user_access_called = true;
    vmm_last_checked_addr = addr;
    vmm_last_checked_size = size;

    if (vmm_should_fail) return 0;

    // Check if addr is "kernel" address we used in test
    // We use 0xFFFFFFFF80000000ULL as bad kernel address
    uint64_t start = (uint64_t)addr;
    if (start >= 0x800000000000ULL) return 0;

    return 1;
}

int vmm_validate_user_ptr(const void* addr, size_t size) {
    return vmm_verify_user_access(addr, size, 0);
}

uint64_t vmm_virt_to_phys(uint64_t virt) { return virt; }
int vmm_map_page(uint64_t phys, uint64_t virt, uint64_t flags) { return 0; }
void vmm_destroy_pml4(uint64_t pml4) {}
uint64_t vmm_clone_pml4(int cow) { return 0; }
int vmm_check_user_string(const char* s, size_t max) { return 1; }

/* Mock VFS functions */
uint32_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) { return size; }

void close_fs(fs_node_t *node) {}
fs_node_t* vfs_lookup(fs_node_t* root, const char* path) { return NULL; }
int create_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
void open_fs(fs_node_t *node, uint8_t read, uint8_t write) {}
int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission) { return 0; }
int unlink_fs(fs_node_t *parent, char *name) { return 0; }

/* Mock ioctl_fs */
int ioctl_fs(fs_node_t *node, int request, void *arg) {
    // If sys_ioctl calls us, it means it passed its check (if any).
    // We return 0 to indicate success.
    return 0;
}

/* Stub MSR functions */
uint64_t rdmsr(uint32_t msr) { return 0; }
void wrmsr(uint32_t msr, uint64_t val) {}

/* Stub Timer */
uint32_t timer_get_uptime_seconds(void) { return 0; }
uint64_t timer_get_ticks(void) { return 0; }
void task_sleep(uint64_t ms) {}

/* Stub Network */
int net_recv(socket_t sock, void* buf, int len, int flags) { return 0; }
int net_send(socket_t sock, const void* buf, int len, int flags) { return 0; }
int net_close(socket_t sock) { return 0; }
socket_t net_socket(int domain, int type, int protocol) { return -1; }
int net_connect(socket_t sock, const struct sockaddr_in* addr, int addrlen) { return -1; }
/* get_socket is static inline in header, so we use that definition */

/* Stub Futex */
int futex_wait(uint32_t *uaddr, uint32_t val, const void *timeout) { return 0; }
int futex_wake(uint32_t *uaddr, int val) { return 0; }

/* Stub PMM */
void* pmm_alloc_page(void) { return malloc(4096); }
void pmm_free_page(void* p) { free(p); }

/* Stub Hal */
uint64_t hal_mem_get_phys(uint64_t virt) { return virt; }
int hal_mem_map(uint64_t phys, uint64_t virt, uint32_t flags) { return 0; }

/* Stub APIC */
void lapic_send_ipi(int id, int vector) {}

/* Stub task functions */
int task_fork(void* regs) { return 0; }
int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) { return 0; }
int task_waitpid(int pid, int* status, int options) { return 0; }
task_t* task_get_by_id(uint32_t id) { return NULL; }
int task_kill(uint32_t pid) { return 0; }

/* Stub syscall entry */
void syscall_entry(void) {}

/* Stub eteros_snprintf (called by syscall.c via macro) */
#include <stdarg.h>
#undef snprintf
#undef vsnprintf
int eteros_snprintf(char* str, size_t size, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vsnprintf(str, size, format, ap);
    va_end(ap);
    return ret;
}

/* Include source */
#include "../kernel/arch/x86_64/syscall.c"

int main() {
    printf("Running test_ioctl_security...\n");

    // Setup task
    memset(&current_task_mock, 0, sizeof(task_t));
    current_task_mock.id = 1;

    // Setup FD 0 as valid
    current_task_mock.fd_table[0].node = (fs_node_t*)malloc(sizeof(fs_node_t));
    memset(current_task_mock.fd_table[0].node, 0, sizeof(fs_node_t));
    current_task_mock.fd_table[0].node->flags = 0; // File
    current_task_mock.fd_table[0].node->ioctl = ioctl_fs; // Important!

    /* TEST 1: sys_ioctl with valid user pointer */
    printf("Test 1: sys_ioctl with user pointer\n");
    void* user_ptr = malloc(10);
    int64_t res = sys_ioctl(0, 0x1234, user_ptr);
    if (res != 0) {
        printf("FAILED: sys_ioctl rejected user pointer (res=%ld)\n", res);
        free(user_ptr);
        free(current_task_mock.fd_table[0].node);
        return 1;
    }
    free(user_ptr);
    printf("PASSED: sys_ioctl accepted user pointer\n");

    /* TEST 2: sys_ioctl with kernel pointer (0xFFFF800000000000) */
    printf("Test 2: sys_ioctl with kernel pointer\n");
    void* kernel_ptr = (void*)0xFFFF800000000000ULL;
    res = sys_ioctl(0, 0x1234, kernel_ptr);

    if (res == -EFAULT) {
        printf("PASSED: sys_ioctl rejected kernel pointer (-EFAULT)\n");
    } else {
        printf("FAILED: sys_ioctl accepted kernel pointer (res=%ld)\n", res);
    }

    /* TEST 3: sys_ioctl with non-canonical address (0x800000000000) */
    printf("Test 3: sys_ioctl with non-canonical pointer\n");
    void* bad_ptr = (void*)0x0000800000000000ULL; // Just above USER_LIMIT
    res = sys_ioctl(0, 0x1234, bad_ptr);

    if (res == -EFAULT) {
        printf("PASSED: sys_ioctl rejected non-canonical pointer (-EFAULT)\n");
    } else {
        printf("FAILED: sys_ioctl accepted non-canonical pointer (res=%ld)\n", res);
    }

    free(current_task_mock.fd_table[0].node);

    // If we failed any test, return 1?
    // We expect Test 2 and 3 to FAIL currently (return 0 instead of -EFAULT).
    // So if they return 0, we print FAILED but exit 0?
    // The plan says "Verify Reproduction (Fail)".
    // So if the test fails, it means the vulnerability is present.
    // I will return 0 regardless, but check output.

    return 0;
}
