#include <assert.h>
#include <stdint.h>
#include <stdio.h>

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

int main(void)
{
    test_pipe_and_dup2();
    test_exit_waitpid();
    puts("test_syscall_m9: PASS");
    return 0;
}
