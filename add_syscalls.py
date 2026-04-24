import re

with open('kernel/arch/x86_64/syscall.c', 'r') as f:
    content = f.read()

# add sys_lstat
sys_lstat_func = """static int64_t sys_lstat(const char* path, struct linux_stat* buf) {
    return sys_newfstatat(AT_FDCWD, path, buf, AT_SYMLINK_NOFOLLOW);
}

"""
content = content.replace("static int64_t sys_stat(const char* path, struct linux_stat* buf) {\n    return sys_newfstatat(AT_FDCWD, path, buf, 0);\n}\n", "static int64_t sys_stat(const char* path, struct linux_stat* buf) {\n    return sys_newfstatat(AT_FDCWD, path, buf, 0);\n}\n\n" + sys_lstat_func)

# add sys_gettimeofday
sys_gettimeofday_func = """static int64_t sys_gettimeofday(struct timeval* tv, struct timezone* tz) {
    (void)tz;
    if (tv) {
        if (!vmm_verify_user_access(tv, sizeof(struct timeval), 1)) return -EFAULT;
        uint64_t ticks = timer_get_ticks();
        uint64_t sec = timer_get_uptime_seconds();
        const uint64_t hz = 100;
        tv->tv_sec = sec;
        tv->tv_usec = (ticks % hz) * (1000000 / hz);
    }
    return 0;
}

"""
content = content.replace("static int64_t sys_clock_gettime(int clock_id, struct timespec* tp) {", sys_gettimeofday_func + "static int64_t sys_clock_gettime(int clock_id, struct timespec* tp) {")

# add sys_getdents
sys_getdents_func = """
// sys_getdents (syscall 78) returns old linux_dirent structure, we just map it to getdents64 for simplicity, or we can use getdents64 logic. Since dirent structure differences might cause issues, let's just map it if we can or implement basic wrapper. Actually userspace libc dirent.c already uses SYS_getdents64. Let's just map 78 to sys_getdents64, this is common in some baremetal systems. Wait, struct sizes are different.
static int64_t sys_getdents(int fd, void* dirp, unsigned int count) {
    // Basic wrapper, in many simple OSes we just map it to getdents64 or return ENOSYS since libc uses getdents64.
    // We can also just return ENOSYS since getdents64 is preferred, but let's implement basic ENOSYS or just map it.
    // For progressive coverage, let's map it. Wait, the ABI for 78 uses struct linux_dirent.
    // Let's implement it.
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    task_t* current = task_get_current();
    if (!current->fd_table[fd].node) return -EBADF;

    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_DIRECTORY) return -ENOTDIR;
    if (!vmm_verify_user_access(dirp, count, 1)) return -EFAULT;

    unsigned int bpos = 0;
    struct dirent entry;

    while (bpos < count) {
        int res = readdir_fs(node, current->fd_table[fd].offset, &entry);
        if (res == -1) break;
        if (res == 1) break;

        int name_len = strlen(entry.name);
        int reclen = 10 + name_len + 2; // d_ino(8) + d_off(4 or 8 in some?) wait old dirent is different.
        // Let's just return -ENOSYS for 78 and let people use 217, or we can do a proper struct linux_dirent.
        // Actually the prompt says "progressive coverage" "priority: what libc and tests use".
        // userspace/libc uses SYS_getdents64 (217). Let's see if anything uses 78.
    }
    return -ENOSYS;
}
"""

with open('kernel/arch/x86_64/syscall.c', 'w') as f:
    f.write(content)
