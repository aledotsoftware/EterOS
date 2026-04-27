with open("include/sched.h", "r") as f:
    content = f.read()

content = content.replace("#define WSTOPPED    WUNTRACED", "#ifndef WSTOPPED\\n#define WSTOPPED    WUNTRACED\\n#endif")

with open("include/sched.h", "w") as f:
    f.write(content)
