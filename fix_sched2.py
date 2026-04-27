with open("include/sched.h", "r") as f:
    content = f.read()

if "WSTOPPED" not in content:
    content = content.replace("#define WUNTRACED   2", "#define WUNTRACED   2\\n#ifndef WSTOPPED\\n#define WSTOPPED    WUNTRACED\\n#endif")

with open("include/sched.h", "w") as f:
    f.write(content)
