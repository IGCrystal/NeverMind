#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "nm/fs.h"
#include "nm/proc.h"
#include "nm/syscall.h"

static void kthread_stub(void *arg)
{
    (void)arg;
}

static void test_pipe_and_dup2(void)
{
    proc_init();
    syscall_init();

    int32_t fds[2] = {-1, -1};
    assert(syscall_dispatch(NM_SYS_PIPE, (uint64_t)(uintptr_t)fds, 0, 0, 0, 0, 0) == 0);
    assert(fds[0] >= 0);
    assert(fds[1] >= 0);

    const char *a = "m9";
    assert(syscall_dispatch(NM_SYS_WRITE, (uint64_t)fds[1], (uint64_t)(uintptr_t)a, 2, 0, 0, 0) == 2);

    assert(syscall_dispatch(NM_SYS_DUP2, (uint64_t)fds[1], 7, 0, 0, 0, 0) == 7);
    const char *b = "!";
    assert(syscall_dispatch(NM_SYS_WRITE, 7, (uint64_t)(uintptr_t)b, 1, 0, 0, 0) == 1);

    char out[8] = {0};
    assert(syscall_dispatch(NM_SYS_READ, (uint64_t)fds[0], (uint64_t)(uintptr_t)out, 3, 0, 0, 0) == 3);
    assert(out[0] == 'm');
    assert(out[1] == '9');
    assert(out[2] == '!');

    assert(syscall_dispatch(NM_SYS_CLOSE, (uint64_t)fds[0], 0, 0, 0, 0, 0) == 0);
    assert(syscall_dispatch(NM_SYS_CLOSE, (uint64_t)fds[1], 0, 0, 0, 0, 0) == 0);
    assert(syscall_dispatch(NM_SYS_CLOSE, 7, 0, 0, 0, 0, 0) == 0);
}

static void test_exit_waitpid(void)
{
    proc_init();
    syscall_init();

    struct nm_task *parent = task_current();
    assert(parent != 0);

    struct nm_task *child = task_create_kernel_thread("child", kthread_stub, 0);
    assert(child != 0);
    int32_t child_pid = child->pid;

    proc_set_current(child);
    assert(syscall_dispatch(NM_SYS_EXIT, 42, 0, 0, 0, 0, 0) == 0);
    assert(child->state == NM_TASK_ZOMBIE);

    proc_set_current(parent);
    int32_t status = -1;
    int64_t got = syscall_dispatch(NM_SYS_WAITPID, (uint64_t)child_pid, (uint64_t)(uintptr_t)&status,
                                   0, 0, 0, 0);
    assert(got == child_pid);
    assert(status == 42);

    assert(syscall_dispatch(NM_SYS_WAITPID, (uint64_t)child_pid, (uint64_t)(uintptr_t)&status,
                            0, 0, 0, 0) == -1);
}

static void test_fork_exec(void)
{
    proc_init();
    syscall_init();
    fs_init();
    assert(fs_mount_root(tmpfs_filesystem()) == 0);

    int fd = fs_open("/sh", NM_O_CREAT | NM_O_RDWR, 0644);
    assert(fd >= 0);
    assert(fs_close(fd) == 0);

    struct nm_task *parent = task_current();
    assert(parent != 0);

    int64_t child_pid = syscall_dispatch(NM_SYS_FORK, 0, 0, 0, 0, 0, 0);
    assert(child_pid > 0);

    struct nm_task *child = task_by_pid((int32_t)child_pid);
    assert(child != 0);
    assert(child->ppid == parent->pid);
    assert(child->regs.rax == 0);

    proc_set_current(child);
    assert(syscall_dispatch(NM_SYS_EXEC, (uint64_t)(uintptr_t)"/sh", 0, 0, 0, 0, 0) == 0);
    assert(child->entry_name != 0);
    assert(child->entry_name[0] == '/');
    assert(child->regs.rip == 0x1100);

    assert(syscall_dispatch(NM_SYS_EXEC, (uint64_t)(uintptr_t)"/not-found", 0, 0, 0, 0, 0) == -1);
}

int main(void)
{
    test_pipe_and_dup2();
    test_exit_waitpid();
    test_fork_exec();
    puts("test_syscall_m9: PASS");
    return 0;
}
