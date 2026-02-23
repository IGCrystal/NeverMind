#include "nm/syscall.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/console.h"
#include "nm/proc.h"

static nm_syscall_handler_t syscall_table[NM_SYSCALL_MAX];

static int64_t sys_getpid(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                          uint64_t a6)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    struct nm_task *cur = task_current();
    if (cur == 0) {
        return -1;
    }
    return cur->pid;
}

static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len, uint64_t a4, uint64_t a5,
                         uint64_t a6)
{
    (void)a4;
    (void)a5;
    (void)a6;

    if (fd != 1 || buf == 0) {
        return -1;
    }

    const char *ptr = (const char *)(uintptr_t)buf;
    for (uint64_t i = 0; i < len; i++) {
        console_putc(ptr[i]);
    }
    return (int64_t)len;
}

void syscall_init(void)
{
    for (size_t i = 0; i < NM_SYSCALL_MAX; i++) {
        syscall_table[i] = 0;
    }

    (void)syscall_register(NM_SYS_GETPID, sys_getpid);
    (void)syscall_register(NM_SYS_WRITE, sys_write);
}

int syscall_register(uint64_t nr, nm_syscall_handler_t fn)
{
    if (nr >= NM_SYSCALL_MAX || fn == 0) {
        return -1;
    }
    syscall_table[nr] = fn;
    return 0;
}

int64_t syscall_dispatch(uint64_t nr, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
                         uint64_t arg5, uint64_t arg6)
{
    if (nr >= NM_SYSCALL_MAX || syscall_table[nr] == 0) {
        return -38;
    }

    return syscall_table[nr](arg1, arg2, arg3, arg4, arg5, arg6);
}
