with open("include/sched.h", "r") as f:
    content = f.read()

content = content.replace("#ifndef WSTOPPED\\n#define WSTOPPED    WUNTRACED\\n#endif", "#define WSTOPPED    WUNTRACED")

with open("include/sched.h", "w") as f:
    f.write(content)
