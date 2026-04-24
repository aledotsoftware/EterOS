import re

with open('kernel/arch/x86_64/syscall.c', 'r') as f:
    lines = f.readlines()

new_lines = []
for line in lines:
    new_lines.append(line)
    if "[5] = (syscall_ptr_t)sys_fstat," in line:
        new_lines.append("    [6] = (syscall_ptr_t)sys_lstat,\n")
    elif "[8] = (syscall_ptr_t)sys_lseek," in line and "sys_poll" in new_lines[-2]:
        # we don't want to duplicate if it's already there
        pass
    elif "[95] = (syscall_ptr_t)sys_umask," in line:
        pass # Wait, let's just use replace_with_git_merge_diff instead for accuracy

with open('kernel/arch/x86_64/syscall.c', 'w') as f:
    f.writelines(new_lines)
