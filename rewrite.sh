sed -i 's/        if (phdr.p_filesz < 2 || phdr.p_filesz > out_interp_size) {/        if (phdr.p_filesz < 2 || phdr.p_filesz >= out_interp_size) {/' kernel/fs/elf.c
sed -i 's/    if (!(flags & 0x03)) {/    if (!(flags \& 0x03)) {\n        flags |= MAP_PRIVATE;\n/' kernel/arch/x86_64/syscall.c
sed -i 's/    if (code == ARCH_GET_FS) {/    if (code == ARCH_GET_FS) {\n        current->fs_base = rdmsr(MSR_FS_BASE);/' kernel/arch/x86_64/syscall.c
sed -i 's/    } else if (code == ARCH_GET_GS) {/    } else if (code == ARCH_GET_GS) {\n        current->gs_base = rdmsr(MSR_KERNEL_GS_BASE);/' kernel/arch/x86_64/syscall.c

sed -i 's/if (sigsetsize != 8) return -EINVAL;/if (sigsetsize != 8) return -EINVAL;/' kernel/arch/x86_64/syscall.c

sed -i 's/    if (sigsetsize != 8) return -EINVAL;/    if (sigsetsize != 8) return -EINVAL;/' kernel/arch/x86_64/syscall.c
sed -i 's/        *oldset = current->signal_mask;/        *oldset = (uint64_t)current->signal_mask;/' kernel/arch/x86_64/syscall.c
sed -i 's/    new_mask_u64 = \*mask;/    new_mask_u64 = \*mask;/' kernel/arch/x86_64/syscall.c
sed -i 's/        current->signal_mask = (uint32_t)new_mask_u64;/        current->signal_mask = (uint32_t)new_mask_u64;/' kernel/arch/x86_64/syscall.c

sed -i 's/static int64_t sys_openat(int dirfd, const char\* path, int flags, int mode) {/static int64_t sys_openat(int dirfd, const char* path, int flags, int mode) {\n    if (!vmm_check_user_string(path, 4096)) return -EFAULT;/' kernel/arch/x86_64/syscall.c
sed -i 's/        kfree(node);/        kfree(node);/' kernel/arch/x86_64/syscall.c

sed -i 's/#define WSTOPPED    WUNTRACED/#ifndef WSTOPPED\n#define WSTOPPED    WUNTRACED\n#endif/' include/sched.h
