import re

with open("userspace/libc/src/syscall.c", "r") as f:
    content = f.read()

content = re.sub(r'static int _set_errno\(long ret\) \{\n    if \(ret < 0\) \{\n        errno = \(int\)\(-ret\);\n        return -1;\n    \}\n    return 0;\n\}', r'', content)

with open("userspace/libc/src/syscall.c", "w") as f:
    f.write(content)
