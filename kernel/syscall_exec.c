#include <types.h>
#include <task.h>
#include <vmm.h>
#include <elf.h>
#include <fs/vfs.h>
#include <string.h>
#include <serial.h>
#include <errno.h>
#include <mm.h>
#include <syscall.h>
#include <cpu.h>
#include <pmm.h>

#define MAX_ARG_SIZE 32768
#define MAX_ARGS     128
#define STACK_TOP    0x7FFFFFFFF000

/* Auxiliary Vector Types */
#define AT_NULL   0
#define AT_PHDR   3
#define AT_PHENT  4
#define AT_PHNUM  5
#define AT_PAGESZ 6
#define AT_BASE   7
#define AT_FLAGS  8
#define AT_ENTRY  9
#define AT_UID    11
#define AT_EUID   12
#define AT_GID    13
#define AT_EGID   14
#define AT_RANDOM 25

typedef struct {
    uint64_t a_type;
    uint64_t a_val;
} Elf64_auxv_t;

/* Helper to validate user string */
static int validate_string(const char* str) {
    if (!str) return 0;
    // Check if pointer is user space
    if (!vmm_is_user_page((uint64_t)str)) return 0;
    return 1;
}

int64_t sys_execve(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) {
    serial_write_string("[EXEC] Syscall execve called for: ");
    if (path) serial_write_string(path);
    serial_write_string("\n");

    if (!validate_string(path)) return -EFAULT;

    /* 1. Capture Arguments from User Space */
    /* We need to copy path, argv, envp to kernel buffers BEFORE destroying address space */

    char* kernel_path = (char*)kmalloc(MAX_ARG_SIZE);
    if (!kernel_path) return -ENOMEM;
    strlcpy(kernel_path, path, MAX_ARG_SIZE);

    /* Allocate buffers for argv strings */
    char** kernel_argv = (char**)kmalloc(sizeof(char*) * (MAX_ARGS + 1));
    if (!kernel_argv) { kfree(kernel_path); return -ENOMEM; }
    memset(kernel_argv, 0, sizeof(char*) * (MAX_ARGS + 1));

    int argc = 0;
    if (argv) {
        /* Validate argv array pointer */
        if (!vmm_is_user_page((uint64_t)argv)) {
            kfree(kernel_argv); kfree(kernel_path); return -EFAULT;
        }

        while (argv[argc] && argc < MAX_ARGS) {
            if (!validate_string(argv[argc])) {
                 // cleanup
                for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
                kfree(kernel_argv); kfree(kernel_path); return -EFAULT;
            }

            char* arg = (char*)kmalloc(MAX_ARG_SIZE);
            if (!arg) {
                for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
                kfree(kernel_argv); kfree(kernel_path); return -ENOMEM;
            }
            strlcpy(arg, argv[argc], MAX_ARG_SIZE);
            kernel_argv[argc] = arg;
            argc++;
        }
    }

    /* Allocate buffers for envp strings */
    char** kernel_envp = (char**)kmalloc(sizeof(char*) * (MAX_ARGS + 1));
    if (!kernel_envp) {
        for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
        kfree(kernel_argv); kfree(kernel_path); return -ENOMEM;
    }
    memset(kernel_envp, 0, sizeof(char*) * (MAX_ARGS + 1));

    int envc = 0;
    if (envp) {
        if (!vmm_is_user_page((uint64_t)envp)) {
             for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
             kfree(kernel_argv); kfree(kernel_envp); kfree(kernel_path); return -EFAULT;
        }

        while (envp[envc] && envc < MAX_ARGS) {
            if (!validate_string(envp[envc])) {
                for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
                for (int i=0; i<envc; i++) kfree(kernel_envp[i]);
                kfree(kernel_argv); kfree(kernel_envp); kfree(kernel_path); return -EFAULT;
            }

            char* env = (char*)kmalloc(MAX_ARG_SIZE);
            if (!env) {
                for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
                for (int i=0; i<envc; i++) kfree(kernel_envp[i]);
                kfree(kernel_argv); kfree(kernel_envp); kfree(kernel_path); return -ENOMEM;
            }
            strlcpy(env, envp[envc], MAX_ARG_SIZE);
            kernel_envp[envc] = env;
            envc++;
        }
    }

    /* 2. Create New Address Space */
    uint64_t old_cr3 = vmm_get_pml4();
    uint64_t new_cr3 = vmm_create_pml4();
    if (!new_cr3) {
         for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
         for (int i=0; i<envc; i++) kfree(kernel_envp[i]);
         kfree(kernel_argv); kfree(kernel_envp); kfree(kernel_path);
         return -ENOMEM;
    }

    /* Switch to new address space */
    vmm_load_pml4(new_cr3);

    /* 3. Load ELF */
    uint64_t phdr_vaddr = 0;
    uint64_t phent_size = 0;
    uint64_t phnum = 0;
    uint64_t entry_point = elf_load_file(kernel_path, 0);

    if (entry_point == 0) {
        serial_write_string("[EXEC] Failed to load ELF\n");
        vmm_load_pml4(old_cr3);
        vmm_destroy_pml4(new_cr3);
        for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
        for (int i=0; i<envc; i++) kfree(kernel_envp[i]);
        kfree(kernel_argv); kfree(kernel_envp); kfree(kernel_path);
        return -ENOEXEC;
    }

    /* Scan headers for PT_PHDR and PT_INTERP */
    fs_node_t* node = vfs_lookup(fs_root, kernel_path);
    if (node) {
        Elf64_Ehdr ehdr;
        read_fs(node, 0, sizeof(Elf64_Ehdr), (uint8_t*)&ehdr);
        phent_size = ehdr.e_phentsize;
        phnum = ehdr.e_phnum;

        /* Find PT_PHDR */
        for (int i=0; i<ehdr.e_phnum; i++) {
             Elf64_Phdr phdr;
             read_fs(node, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(Elf64_Phdr), (uint8_t*)&phdr);
             if (phdr.p_type == PT_PHDR) {
                 phdr_vaddr = phdr.p_vaddr;
             }
        }

        /* Check PT_INTERP */
        char interp_path[128] = {0};
        for (int i=0; i<ehdr.e_phnum; i++) {
             Elf64_Phdr phdr;
             read_fs(node, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(Elf64_Phdr), (uint8_t*)&phdr);
             if (phdr.p_type == PT_INTERP) {
                 if (phdr.p_filesz < 128) {
                     read_fs(node, phdr.p_offset, phdr.p_filesz, (uint8_t*)interp_path);
                     interp_path[phdr.p_filesz] = 0;
                 }
             }
        }
        kfree(node);

        if (interp_path[0] != 0) {
            serial_write_string("[EXEC] Interpreter: ");
            serial_write_string(interp_path);
            serial_write_string("\n");

            uint64_t interp_base = 0x40000000;
            uint64_t interp_entry = elf_load_file(interp_path, interp_base);
            if (interp_entry != 0) {
                entry_point = interp_entry + interp_base;
            } else {
                serial_write_string("[EXEC] Failed to load interpreter!\n");
                vmm_load_pml4(old_cr3);
                vmm_destroy_pml4(new_cr3);
                 for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
                 for (int i=0; i<envc; i++) kfree(kernel_envp[i]);
                 kfree(kernel_argv); kfree(kernel_envp); kfree(kernel_path);
                return -ENOEXEC;
            }
        }
    }

    /* 4. Setup Stack */
    /* Map 4 pages for stack */
    for (int i=0; i<4; i++) {
        uint64_t page_addr = STACK_TOP - (i+1)*PAGE_SIZE;
        void* phys = pmm_alloc_page();
        if (!phys) {
            vmm_load_pml4(old_cr3);
            vmm_destroy_pml4(new_cr3);
             for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
             for (int i=0; i<envc; i++) kfree(kernel_envp[i]);
             kfree(kernel_argv); kfree(kernel_envp); kfree(kernel_path);
            return -ENOMEM;
        }
        memset(phys, 0, PAGE_SIZE);
        vmm_map_page((uint64_t)phys, page_addr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    }

    uint64_t sp = STACK_TOP;

    uint64_t argv_ptrs[MAX_ARGS+1];
    uint64_t envp_ptrs[MAX_ARGS+1];

    /* Push Envp Strings */
    for (int i=envc-1; i>=0; i--) {
        size_t len = strlen(kernel_envp[i]) + 1;
        sp -= len;
        strlcpy((char*)sp, kernel_envp[i], len);
        envp_ptrs[i] = sp;
    }

    /* Push Argv Strings */
    for (int i=argc-1; i>=0; i--) {
        size_t len = strlen(kernel_argv[i]) + 1;
        sp -= len;
        strlcpy((char*)sp, kernel_argv[i], len);
        argv_ptrs[i] = sp;
    }

    /* Align stack to 16 bytes */
    sp &= ~0xF;

    /* Push Aux Vectors */
    Elf64_auxv_t auxv[] = {
        {AT_PHDR, phdr_vaddr},
        {AT_PHENT, phent_size},
        {AT_PHNUM, phnum},
        {AT_PAGESZ, PAGE_SIZE},
        {AT_ENTRY, entry_point},
        {AT_UID, 0},
        {AT_EUID, 0},
        {AT_GID, 0},
        {AT_EGID, 0},
        {AT_NULL, 0}
    };

    int auxv_count = sizeof(auxv)/sizeof(Elf64_auxv_t);
    sp -= auxv_count * sizeof(Elf64_auxv_t);
    memcpy((void*)sp, auxv, auxv_count * sizeof(Elf64_auxv_t));

    /* Push Envp Pointers */
    sp -= sizeof(uint64_t); // NULL terminator
    *(uint64_t*)sp = 0;

    for (int i=envc-1; i>=0; i--) {
        sp -= sizeof(uint64_t);
        *(uint64_t*)sp = envp_ptrs[i];
    }

    /* Push Argv Pointers */
    sp -= sizeof(uint64_t); // NULL terminator
    *(uint64_t*)sp = 0;

    for (int i=argc-1; i>=0; i--) {
        sp -= sizeof(uint64_t);
        *(uint64_t*)sp = argv_ptrs[i];
    }

    /* Push Argc */
    sp -= sizeof(uint64_t);
    *(uint64_t*)sp = argc;

    /* 5. Cleanup Kernel Buffers */
    for (int i=0; i<argc; i++) kfree(kernel_argv[i]);
    for (int i=0; i<envc; i++) kfree(kernel_envp[i]);
    kfree(kernel_argv); kfree(kernel_envp); kfree(kernel_path);

    /* 6. Commit */
    task_t* current = task_get_current();
    current->cr3 = new_cr3;
    current->brk = 0;
    current->entry = (void (*)(void))entry_point;
    current->user_rsp = sp;

    /* Reset Signals */
    current->signal_mask = 0;
    current->signal_pending = 0;
    memset(current->signal_handlers, 0, sizeof(current->signal_handlers));

    vmm_destroy_pml4(old_cr3);

    /* Update Regs for return */
    regs->rcx = entry_point; /* RIP */
    /* regs->rsp is not used by sysret */
    regs->r11 = 0x202;       /* RFLAGS */

    cpu_info_t* cpu = get_current_cpu();
    if (cpu) {
        cpu->user_stack_scratch = sp;
    }

    return 0;
}
