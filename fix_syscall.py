import re

with open("userspace/libc/src/syscall.c", "r") as f:
    content = f.read()

content = re.sub(r'    return _set_errno\(ret\);\n    return \(.*\)ret;', r'    return _set_errno(ret);', content)
content = re.sub(r'    return _set_errno\(ret\);\n    return -1;', r'    return _set_errno(ret);', content)
content = re.sub(r'    return _set_errno\(ret\);\n    return 0;', r'    return _set_errno(ret);', content)

with open("userspace/libc/src/syscall.c", "w") as f:
    f.write(content)
