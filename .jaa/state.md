## EterOS Linux Syscall Compliance Update (Current Run)
- Mapped missing Linux x86_64 syscalls (`SYS_pread64`, `SYS_pwrite64`, `SYS_select`, `SYS_poll`, `SYS_sysinfo`, `SYS_rmdir`) to both `syscall_linux_table` and `syscall_native_table` in `kernel/arch/x86_64/syscall.c` to improve GNU utilities coverage.
- Implemented a dedicated `sys_rmdir` wrapper that correctly enforces directory type checks (`FS_DIRECTORY`) before executing the VFS `unlink_fs` operation, preventing file deletion through `rmdir`.
- Fixed `SYS_rmdir` 32-bit mapping by placing it at index 40 in `syscall_linux32_table`, preventing a severe ABI conflict where index 84 (`oldlstat`) was accidentally mapped to `sys_unlink`.
- Patched a memory leak and potential use-after-free issue in `sys_unlinkat` and the new `sys_rmdir` by correctly using `close_fs(node)` on dynamically allocated VFS nodes instead of raw `kfree()` calls, avoiding kernel memory corruption.
