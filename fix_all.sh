cat << 'INNER_EOF' > fix.py
import sys

def patch_elf():
    with open("kernel/fs/elf.c", "r") as f:
        content = f.read()
    content = content.replace("if (phdr.p_filesz < 2 || phdr.p_filesz > out_interp_size)", "if (phdr.p_filesz < 2 || phdr.p_filesz >= out_interp_size)")
    with open("kernel/fs/elf.c", "w") as f:
        f.write(content)

def patch_syscall():
    with open("kernel/arch/x86_64/syscall.c", "r") as f:
        content = f.read()

    content = content.replace(
"""    if (!(flags & 0x03)) {
        if (current && current->os_abi == ELFOSABI_LINUX && fd != -1) {
            /* Allow file-backed mmap without MAP_ANONYMOUS in Linux ABI */
            flags |= MAP_PRIVATE;
        } else {
            return -ENODEV;
        }
    }""",
"""    if (!(flags & 0x03)) {
        flags |= MAP_PRIVATE;
    }"""
    )

    content = content.replace(
"""static int64_t sys_arch_prctl(int code, uint64_t addr) {
    task_t* current = task_get_current();
    if (code == ARCH_SET_FS) {
        current->fs_base = addr;
        wrmsr(MSR_FS_BASE, addr);
        return 0;
    } else if (code == ARCH_SET_GS) {
        current->gs_base = addr;
        wrmsr(MSR_KERNEL_GS_BASE, addr);
        return 0;
    } else if (code == ARCH_GET_FS) {
        if (!vmm_verify_user_access((void*)addr, sizeof(uint64_t), 1)) return -EFAULT;
        *(uint64_t*)addr = current->fs_base;
        return 0;
    } else if (code == ARCH_GET_GS) {
        if (!vmm_verify_user_access((void*)addr, sizeof(uint64_t), 1)) return -EFAULT;
        *(uint64_t*)addr = current->gs_base;
        return 0;
    }
    return -EINVAL;
}""",
"""static int64_t sys_arch_prctl(int code, uint64_t addr) {
    task_t* current = task_get_current();
    if (code == ARCH_SET_FS) {
        current->fs_base = addr;
        wrmsr(MSR_FS_BASE, addr);
        return 0;
    } else if (code == ARCH_SET_GS) {
        current->gs_base = addr;
        wrmsr(MSR_KERNEL_GS_BASE, addr);
        return 0;
    } else if (code == ARCH_GET_FS) {
        if (!vmm_verify_user_access((void*)addr, sizeof(uint64_t), 1)) return -EFAULT;
        *(uint64_t*)addr = rdmsr(MSR_FS_BASE);
        return 0;
    } else if (code == ARCH_GET_GS) {
        if (!vmm_verify_user_access((void*)addr, sizeof(uint64_t), 1)) return -EFAULT;
        *(uint64_t*)addr = rdmsr(MSR_KERNEL_GS_BASE);
        return 0;
    }
    return -EINVAL;
}"""
    )

    content = content.replace(
"""static int64_t sys_rt_sigaction(int signum, const struct sigaction* act, struct sigaction* oldact, size_t sigsetsize) {
    if (signum < 1 || signum >= 32 || signum == SIGKILL || signum == SIGSTOP) return -EINVAL;
    if (sigsetsize != 8) return -EINVAL;""",
"""static int64_t sys_rt_sigaction(int signum, const struct sigaction* act, struct sigaction* oldact, size_t sigsetsize) {
    if (signum < 1 || signum >= 64 || signum == SIGKILL || signum == SIGSTOP) return -EINVAL;
    if (sigsetsize != 8) return -EINVAL;"""
    )

    content = content.replace(
"""static int64_t sys_rt_sigprocmask(int how, const uint64_t* set, uint64_t* oldset, size_t sigsetsize) {
    if (sigsetsize != 8) return -EINVAL;
    task_t* current = task_get_current();
    if (oldset) {
        if (!vmm_verify_user_access(oldset, sizeof(uint64_t), 1)) return -EFAULT;
        *oldset = current->signal_mask;
    }
    if (set) {
        if (!vmm_verify_user_access(set, sizeof(uint64_t), 0)) return -EFAULT;
        if (how == SIG_BLOCK) current->signal_mask |= *set;
        else if (how == SIG_UNBLOCK) current->signal_mask &= ~(*set);
        else if (how == SIG_SETMASK) current->signal_mask = *set;
        else return -EINVAL;
        current->signal_mask &= ~((1ULL << (SIGKILL - 1)) | (1ULL << (SIGSTOP - 1)));
    }
    return 0;
}""",
"""static int64_t sys_rt_sigprocmask(int how, const uint64_t* set, uint64_t* oldset, size_t sigsetsize) {
    if (sigsetsize != 8) return -EINVAL;
    task_t* current = task_get_current();
    if (oldset) {
        if (!vmm_verify_user_access(oldset, sizeof(uint64_t), 1)) return -EFAULT;
        *oldset = current->signal_mask;
    }
    if (set) {
        if (!vmm_verify_user_access(set, sizeof(uint64_t), 0)) return -EFAULT;
        if (how == SIG_BLOCK) current->signal_mask |= *set;
        else if (how == SIG_UNBLOCK) current->signal_mask &= ~(*set);
        else if (how == SIG_SETMASK) current->signal_mask = *set;
        else return -EINVAL;
        current->signal_mask &= ~((1ULL << (SIGKILL - 1)) | (1ULL << (SIGSTOP - 1)));
    }
    return 0;
}"""
    )

    content = content.replace(
"""static int64_t sys_openat(int dirfd, const char* path, int flags, int mode) {
    fs_node_t* node = NULL;""",
"""static int64_t sys_openat(int dirfd, const char* path, int flags, int mode) {
    if (!vmm_check_user_string(path, 4096)) return -EFAULT;
    fs_node_t* node = NULL;"""
    )

    content = content.replace(
"""static int64_t sys_readlinkat(int dirfd, const char* path, char* buf, size_t bufsiz) {
    if (!vmm_verify_user_access(buf, bufsiz, 1)) return -EFAULT;""",
"""static int64_t sys_readlinkat(int dirfd, const char* path, char* buf, size_t bufsiz) {
    if (!vmm_verify_user_access(buf, bufsiz, 1)) return -EFAULT;
    if (!vmm_check_user_string(path, 4096)) return -EFAULT;"""
    )

    content = content.replace(
"""    if (read_bytes >= 0 && read_bytes < (ssize_t)bufsiz) {
        buf[read_bytes] = '\0';
    }""",
"""    if (read_bytes >= 0 && read_bytes < (ssize_t)bufsiz) {
        buf[read_bytes] = '\0';
    }"""
    )

    rmdir_func = """static int64_t sys_rmdir(const char* path) {
    if (!vmm_verify_user_access(path, 1, 0)) return -EFAULT;
    char kpath[256];
    if (vmm_strncpy_from_user(kpath, path, sizeof(kpath)) < 0) return -EFAULT;
    fs_node_t* node = vfs_lookup(fs_root, kpath);
    if (!node) return -ENOENT;
    if ((node->flags & 0x7) != FS_DIRECTORY) {
        kfree(node);
        return -ENOTDIR;
    }
    kfree(node);
    return sys_unlinkat(AT_FDCWD, path, 0x200);
}

"""
    if "sys_rmdir" not in content:
        content = content.replace("static int64_t sys_unlink(const char* path) {", rmdir_func + "static int64_t sys_unlink(const char* path) {")
        content = content.replace("    [84] = (syscall_ptr_t)sys_ni_syscall,", "    [84] = (syscall_ptr_t)sys_rmdir,")
        content = content.replace("    [40] = (syscall_ptr_t)sys_ni_syscall,", "    [40] = (syscall_ptr_t)sys_rmdir,")

        # Manually add to tables
        content = content.replace("    [87] = (syscall_ptr_t)sys_unlink,", "    [84] = (syscall_ptr_t)sys_rmdir,\n    [87] = (syscall_ptr_t)sys_unlink,")
        content = content.replace("    [10] = (syscall_ptr_t)sys_unlink,", "    [40] = (syscall_ptr_t)sys_rmdir,\n    [10] = (syscall_ptr_t)sys_unlink,")

    content = content.replace(
"""static int64_t sys_fstat(int fd, struct linux_stat* buf) {
    task_t* current = task_get_current();""",
"""static int64_t sys_fstat(int fd, struct linux_stat* buf) {
    if (!vmm_verify_user_access(buf, sizeof(struct linux_stat), 1)) return -EFAULT;
    task_t* current = task_get_current();"""
    )

    content = content.replace(
"""static int64_t sys_newfstatat(int dirfd, const char* path, struct linux_stat* buf, int flags) {
    const int allowed_flags = AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH;
    fs_node_t* node = NULL;
    char* kpath;
    char user_path[256];

    if ((flags & ~allowed_flags) != 0) return -EINVAL;
    if (!path) return -EFAULT;""",
"""static int64_t sys_newfstatat(int dirfd, const char* path, struct linux_stat* buf, int flags) {
    const int allowed_flags = AT_SYMLINK_NOFOLLOW | AT_EMPTY_PATH;
    fs_node_t* node = NULL;
    char* kpath;
    char user_path[256];

    if ((flags & ~allowed_flags) != 0) return -EINVAL;
    if (!vmm_verify_user_access(buf, sizeof(struct linux_stat), 1)) return -EFAULT;
    if (!path) return -EFAULT;"""
    )

    with open("kernel/arch/x86_64/syscall.c", "w") as f:
        f.write(content)

def patch_task():
    with open("kernel/task.c", "r") as f:
        content = f.read()

    content = content.replace(
"""    if (clone_flags & CLONE_CHILD_SETTID) {
        if (child_tid) {
             /* Handled by user space, but can be done here. */
        }
    }""",
"""    if (clone_flags & CLONE_CHILD_SETTID) {
        if (child_tid) {
             if (vmm_verify_user_access(child_tid, sizeof(uint32_t), 1)) {
                  *child_tid = tasks[slot].id;
             }
        }
    }"""
    )
    with open("kernel/task.c", "w") as f:
        f.write(content)

patch_elf()
patch_syscall()
patch_task()
INNER_EOF
python3 fix.py
cat << 'INNER_EOF' > fix_sched.py
with open("include/sched.h", "r") as f:
    content = f.read()
content = content.replace("#define WSTOPPED    WUNTRACED", "#ifndef WSTOPPED\\n#define WSTOPPED    WUNTRACED\\n#endif")
with open("include/sched.h", "w") as f:
    f.write(content)
INNER_EOF
python3 fix_sched.py
