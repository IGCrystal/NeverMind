#include "nm/syscall.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/console.h"
#include "nm/errno.h"
#include "nm/exec.h"
#include "nm/fd.h"
#include "nm/fs.h"
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

    const struct nm_task *cur = task_current();
    if (cur == 0) {
        return NM_ERR(NM_EFAIL);
    }
    return cur->pid;
}

static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len, uint64_t a4, uint64_t a5,
                         uint64_t a6)
{
    (void)a4;
    (void)a5;
    (void)a6;

    if (buf == 0) {
        return NM_ERR(NM_EINVAL);
    }

    struct nm_task *cur = task_current();
    if (cur == 0) {
        return NM_ERR(NM_EFAIL);
    }

    int64_t n = nm_fd_write(cur, (int32_t)fd, (const void *)(uintptr_t)buf, len);
    if (n >= 0) {
        return n;
    }

    if (fd != 1) {
        return NM_ERR(NM_EFAIL);
    }

#ifdef NEVERMIND_HOST_TEST
    return (int64_t)len;
#else
    const char *ptr = (const char *)(uintptr_t)buf;
    for (uint64_t i = 0; i < len; i++) {
        console_putc(ptr[i]);
    }
    return (int64_t)len;
#endif
}

static int64_t sys_read(uint64_t fd, uint64_t buf, uint64_t len, uint64_t a4, uint64_t a5,
                        uint64_t a6)
{
    (void)a4;
    (void)a5;
    (void)a6;

    if (buf == 0) {
        return NM_ERR(NM_EINVAL);
    }

    struct nm_task *cur = task_current();
    if (cur == 0) {
        return NM_ERR(NM_EFAIL);
    }

    return nm_fd_read(cur, (int32_t)fd, (void *)(uintptr_t)buf, len);
}

static int64_t sys_close(uint64_t fd, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                         uint64_t a6)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    struct nm_task *cur = task_current();
    if (cur == 0 || fd >= (uint64_t)NM_MAX_FDS) {
        return NM_ERR(NM_EFAIL);
    }

    return nm_fd_close(cur, (int32_t)fd);
}

static int64_t sys_exit(uint64_t code, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                        uint64_t a6)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    proc_exit_current((int32_t)code);
    return 0;
}

static int64_t sys_waitpid(uint64_t pid, uint64_t status_ptr, uint64_t a3, uint64_t a4,
                           uint64_t a5, uint64_t a6)
{
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    int32_t status = 0;
    int32_t got = proc_waitpid((int32_t)pid, &status);
    if (got < 0) {
        return NM_ERR(NM_EFAIL);
    }

    if (status_ptr != 0) {
        int32_t *user_status = (int32_t *)(uintptr_t)status_ptr;
        *user_status = status;
    }
    return got;
}

static int64_t sys_pipe(uint64_t fds_ptr, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                        uint64_t a6)
{
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    struct nm_task *cur = task_current();
    if (cur == 0 || fds_ptr == 0) {
        return NM_ERR(NM_EINVAL);
    }

    int32_t rd = -1;
    int32_t wr = -1;
    if (nm_fd_pipe(cur, &rd, &wr) != 0) {
        return NM_ERR(NM_EFAIL);
    }

    int32_t *fds = (int32_t *)(uintptr_t)fds_ptr;
    fds[0] = rd;
    fds[1] = wr;
    return 0;
}

static int64_t sys_dup2(uint64_t oldfd, uint64_t newfd, uint64_t a3, uint64_t a4, uint64_t a5,
                        uint64_t a6)
{
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    struct nm_task *cur = task_current();
    if (cur == 0 || oldfd >= (uint64_t)NM_MAX_FDS || newfd >= (uint64_t)NM_MAX_FDS) {
        return NM_ERR(NM_EFAIL);
    }

    return nm_fd_dup2(cur, (int32_t)oldfd, (int32_t)newfd);
}

static int64_t sys_fd_cloexec(uint64_t fd, uint64_t enabled, uint64_t a3, uint64_t a4, uint64_t a5,
                              uint64_t a6)
{
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    struct nm_task *cur = task_current();
    if (cur == 0 || fd >= (uint64_t)NM_MAX_FDS) {
        return NM_ERR(NM_EFAIL);
    }

    if (enabled == 0 || enabled == 1) {
        return nm_fd_set_cloexec(cur, (int32_t)fd, (int)enabled);
    }

    if (enabled == 2) {
        return nm_fd_get_cloexec(cur, (int32_t)fd);
    }

    return NM_ERR(NM_EINVAL);
}

static int64_t sys_fork(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                        uint64_t a6)
{
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    struct nm_task *parent = task_current();
    if (parent == 0) {
        return NM_ERR(NM_EFAIL);
    }

    struct nm_task *child = proc_fork_current();
    if (child == 0) {
        return NM_ERR(NM_EFAIL);
    }

    (void)nm_fd_on_fork_child(child);

    parent->regs.rax = (uint64_t)child->pid;
    return child->pid;
}

static int64_t sys_exec(uint64_t name_ptr, uint64_t argv_ptr, uint64_t envp_ptr, uint64_t a4,
                        uint64_t a5,
                        uint64_t a6)
{
    (void)a5;
    (void)a6;

    if (name_ptr == 0) {
        return NM_ERR(NM_EFAIL);
    }

    const char *name = (const char *)(uintptr_t)name_ptr;
    struct nm_stat st;
    if (fs_stat(name, &st) != 0) {
        return NM_ERR(NM_EFAIL);
    }

    const char *const *argv = (const char *const *)(uintptr_t)argv_ptr;
    const char *const *envp = (const char *const *)(uintptr_t)envp_ptr;

    uint64_t final_entry = 0;
    if (nm_exec_resolve_entry(name, &final_entry) != 0) {
        final_entry = 0;
    }

    if (argv_ptr != 0 && envp_ptr == 0 && a4 == 0 && argv_ptr < 0x100000ULL) {
        final_entry = argv_ptr;
        argv = 0;
    } else if (a4 != 0) {
        final_entry = a4;
    }

    if (final_entry == 0) {
        return NM_ERR(NM_EFAIL);
    }

    nm_fd_close_on_exec(task_current());
    return proc_exec_current(name, final_entry, argv, envp);
}

void syscall_init(void)
{
    for (size_t i = 0; i < NM_SYSCALL_MAX; i++) {
        syscall_table[i] = 0;
    }

    nm_fd_init();

    (void)syscall_register(NM_SYS_GETPID, sys_getpid);
    (void)syscall_register(NM_SYS_WRITE, sys_write);
    (void)syscall_register(NM_SYS_READ, sys_read);
    (void)syscall_register(NM_SYS_CLOSE, sys_close);
    (void)syscall_register(NM_SYS_EXIT, sys_exit);
    (void)syscall_register(NM_SYS_WAITPID, sys_waitpid);
    (void)syscall_register(NM_SYS_PIPE, sys_pipe);
    (void)syscall_register(NM_SYS_DUP2, sys_dup2);
    (void)syscall_register(NM_SYS_FORK, sys_fork);
    (void)syscall_register(NM_SYS_EXEC, sys_exec);
    (void)syscall_register(NM_SYS_FD_CLOEXEC, sys_fd_cloexec);
}

int syscall_register(uint64_t nr, nm_syscall_handler_t fn)
{
    if (nr >= NM_SYSCALL_MAX || fn == 0) {
        return NM_ERR(NM_EFAIL);
    }
    syscall_table[nr] = fn;
    return 0;
}

int64_t syscall_dispatch(uint64_t nr, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
                         uint64_t arg5, uint64_t arg6)
{
    if (nr >= NM_SYSCALL_MAX || syscall_table[nr] == 0) {
        return NM_ERR(NM_ENOSYS);
    }

    return syscall_table[nr](arg1, arg2, arg3, arg4, arg5, arg6);
}
