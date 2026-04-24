import re

with open('kernel/arch/x86_64/syscall.c', 'r') as f:
    content = f.read()

# Replace struct timezone* tz with void* tz to avoid warning
content = content.replace("static int64_t sys_gettimeofday(struct timeval* tv, struct timezone* tz) {", "static int64_t sys_gettimeofday(struct timeval* tv, void* tz) {")

with open('kernel/arch/x86_64/syscall.c', 'w') as f:
    f.write(content)
