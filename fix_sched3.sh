sed -i 's/#define WSTOPPED    WUNTRACED/#ifndef WSTOPPED\n#define WSTOPPED    WUNTRACED\n#endif/' include/sched.h
