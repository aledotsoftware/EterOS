#!/bin/bash
cat << 'MD' >> .jaa/state.md
- **linux-syscall-compliance-bot**: Fixed `sys_getrusage` and `sys_prlimit64` to strictly return `-ENOSYS` without modifying user-provided memory buffers (`usage` and `old_limit` respectively), adhering to POSIX ABI compliance. Added memory modification assertions to `test_syscall_linux_coverage.c` to prevent future regressions.
MD
