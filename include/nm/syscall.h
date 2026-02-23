#ifndef NM_SYSCALL_H
#define NM_SYSCALL_H

#include <stdint.h>

#define NM_SYSCALL_MAX 128

enum nm_syscall_nr {
    NM_SYS_GETPID = 0,
    NM_SYS_WRITE = 1,
    NM_SYS_READ = 2,
    NM_SYS_CLOSE = 3,
    NM_SYS_EXIT = 4,
    NM_SYS_WAITPID = 5,
    NM_SYS_PIPE = 6,
    NM_SYS_DUP2 = 7,
    NM_SYS_FORK = 8,
    NM_SYS_EXEC = 9,
    NM_SYS_FD_CLOEXEC = 10,
};

typedef int64_t (*nm_syscall_handler_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

void syscall_init(void);
int syscall_register(uint64_t nr, nm_syscall_handler_t fn);
int64_t syscall_dispatch(uint64_t nr, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
                         uint64_t arg5, uint64_t arg6);

#endif
