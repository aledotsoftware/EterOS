cat kernel/fs/devfs.c | grep -n -B 5 -A 40 "if (tr.data.ptr.buffer && tr.data_size > 0 && target_task && target_task->binder_mmap_base != 0) {"
