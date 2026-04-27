#!/bin/bash
find userspace/libc/src/ -name "*.c" -exec sed -i -E 's/if \(ret < 0\) \{ errno = \(int\)\(-ret\); return -1; \}/return _set_errno(ret);/g' {} +
