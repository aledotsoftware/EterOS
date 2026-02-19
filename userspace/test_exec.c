#include <stdint.h>
#include <stddef.h>

#define SYS_write 1
#define SYS_fork 57
#define SYS_execve 59
#define SYS_exit 60
#define SYS_wait4 61
#define SYS_kill 62
#define SYS_rt_sigaction 13
#define SYS_rt_sigreturn 15
#define SYS_getpid 39

#define SIGUSR1 10

typedef struct {
    void     (*handler)(int);
    uint64_t  flags;
    void     (*restorer)(void);
    uint64_t  mask;
} kernel_sigaction_t;

/* Functions from libc */
extern void exit(int status);
extern int getpid(void);
extern int kill(int pid, int sig);
typedef long ssize_t;
extern ssize_t write(int fd, const void *buf, size_t count);

static inline long syscall0(long n) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall1(long n, long a1) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall2(long n, long a1, long a2) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall3(long n, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

static inline long syscall4(long n, long a1, long a2, long a3, long a4) {
    long ret;
    register long r10 __asm__("r10") = a4;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}

void print(const char* s) {
    long len = 0;
    while(s[len]) len++;
    write(1, s, len);
}

int fork() {
    return (int)syscall0(SYS_fork);
}

int execve(const char* path, char* const argv[], char* const envp[]) {
    return (int)syscall3(SYS_execve, (long)path, (long)argv, (long)envp);
}

__attribute__((naked))
void __restore_rt(void) {
    __asm__ volatile (
        "mov $15, %rax\n\t"
        "syscall\n\t"
    );
}

int sigaction(int sig, const kernel_sigaction_t* act, kernel_sigaction_t* oldact) {
    return (int)syscall4(SYS_rt_sigaction, sig, (long)act, (long)oldact, 8);
}

volatile int signal_received = 0;

void my_handler(int sig) {
    (void)sig;
    print("Signal Handler: Caught Signal!\n");
    signal_received = 1;
}

int main(int argc, char** argv) {
    if (argc > 1) {
        print("Child: Exec Success! Argv[1]=");
        print(argv[1]);
        print("\n");

        print("Child: Sending Signal to self...\n");

        /* Register Handler */
        kernel_sigaction_t act;
        act.handler = my_handler;
        act.flags = 0;
        act.restorer = __restore_rt;
        act.mask = 0;

        sigaction(SIGUSR1, &act, 0);

        kill(getpid(), SIGUSR1);

        // Wait loop to allow signal to process
        for(volatile int i=0; i<100000; i++);

        if (signal_received) {
            print("Child: Signal Verified.\n");
        } else {
            print("Child: Signal FAILED.\n");
        }

        exit(0);
    } else {
        print("Parent: Forking...\n");
        int pid = fork();
        if (pid == 0) {
            char* args[] = {"/test_exec.elf", "Success", 0};
            print("Child: Executing self...\n");
            execve("/test_exec.elf", args, 0);
            print("Child: Exec FAILED.\n");
            exit(1);
        } else {
            print("Parent: Waiting...\n");
            syscall4(SYS_wait4, pid, 0, 0, 0);
            print("Parent: Child exited. Test Complete.\n");
        }
    }
    return 0;
}
