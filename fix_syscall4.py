import os

with open("userspace/libc/src/syscall.c", "r") as f:
    content = f.read()

content = "extern int _set_errno(long ret);\n" + content

with open("userspace/libc/src/syscall.c", "w") as f:
    f.write(content)

with open("userspace/libc/src/signal.c", "r") as f:
    content = f.read()

content = "extern int _set_errno(long ret);\n" + content

with open("userspace/libc/src/signal.c", "w") as f:
    f.write(content)
