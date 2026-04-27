#!/bin/bash
sed -i 's/int _set_errno/extern int _set_errno/g' userspace/libc/src/syscall.c
sed -i 's/int _set_errno/extern int _set_errno/g' userspace/libc/src/signal.c
