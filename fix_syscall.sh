#!/bin/bash
# Userspace string functions
cat << 'INNER_EOF' > fix_file.py
import re

with open("userspace/libc/src/syscall.c", "r") as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    if "return _set_errno(ret);" in line and "return (int)ret;" in line:
        pass
    new_lines.append(line)

# Since doing it carefully, let's just rewrite the return macros
INNER_EOF
python3 fix_file.py
