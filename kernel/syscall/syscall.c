#include "nm/syscall.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/console.h"
#include "nm/fs.h"
#include "nm/proc.h"

static nm_syscall_handler_t syscall_table[NM_SYSCALL_MAX];

#define NM_PIPE_MAX 16
#define NM_PIPE_BUF 512

struct nm_pipe {
    int used;
    uint8_t buf[NM_PIPE_BUF];
    uint64_t read_pos;
    uint64_t write_pos;
    uint32_t readers;
    uint32_t writers;
};

static struct nm_pipe pipe_table[NM_PIPE_MAX];

static int32_t encode_pipe_fd(int pipe_id, int is_write_end)
{
    return -(1000 + (pipe_id * 2) + (is_write_end ? 1 : 0));
}

static int decode_pipe_fd(int32_t value, int *pipe_id, int *is_write_end)
{
    if (value > -1000) {
        return 0;
    }

    int raw = -value - 1000;
    int id = raw / 2;
    int w = raw % 2;
    if (id < 0 || id >= NM_PIPE_MAX) {
        return 0;
    }
    if (pipe_id) {
        *pipe_id = id;
    }
    if (is_write_end) {
        *is_write_end = w;
    }
    return 1;
}

static int alloc_task_fd(struct nm_task *task)
{
    if (task == 0) {
        return -1;
    }
    for (int i = 0; i < NM_MAX_FDS; i++) {
        if (task->fd_table[i] == -1) {
            return i;
        }
    }
    return -1;
}

static int close_task_fd_value(int32_t value)
{
    int pipe_id = 0;
    int is_write_end = 0;
    if (decode_pipe_fd(value, &pipe_id, &is_write_end)) {
        struct nm_pipe *pipe = &pipe_table[pipe_id];
        if (!pipe->used) {
            return 0;
        }
        if (is_write_end) {
            if (pipe->writers > 0) {
                pipe->writers--;
            }
        } else {
            if (pipe->readers > 0) {
                pipe->readers--;
            }
        }
        if (pipe->readers == 0 && pipe->writers == 0) {
            pipe->used = 0;
            pipe->read_pos = 0;
            pipe->write_pos = 0;
        }
        return 0;
    }

    if (value >= 0) {
        return fs_close(value);
    }
    return -1;
}

static int32_t get_mapped_fd(struct nm_task *task, int32_t fd)
{
    if (task == 0) {
        return -1;
    }
    if (fd < 0 || fd >= NM_MAX_FDS) {
        return -1;
    }
    return task->fd_table[fd];
}

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

    if (buf == 0) {
        return -1;
    }

    struct nm_task *cur = task_current();
    if (cur == 0) {
        return -1;
    }

    const char *ptr = (const char *)(uintptr_t)buf;

    int32_t mapped = get_mapped_fd(cur, (int32_t)fd);
    int pipe_id = 0;
    int is_write_end = 0;
    if (decode_pipe_fd(mapped, &pipe_id, &is_write_end)) {
        if (!is_write_end) {
            return -1;
        }
        struct nm_pipe *pipe = &pipe_table[pipe_id];
        if (!pipe->used) {
            return -1;
        }

        uint64_t written = 0;
        while (written < len && (pipe->write_pos - pipe->read_pos) < NM_PIPE_BUF) {
            pipe->buf[pipe->write_pos % NM_PIPE_BUF] = (uint8_t)ptr[written];
            pipe->write_pos++;
            written++;
        }
        return (int64_t)written;
    }

    if (mapped >= 0) {
        return fs_write(mapped, ptr, len);
    }

    if (fd != 1) {
        return -1;
    }

#ifdef NEVERMIND_HOST_TEST
    return (int64_t)len;
#else
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
        return -1;
    }

    struct nm_task *cur = task_current();
    if (cur == 0) {
        return -1;
    }

    int32_t mapped = get_mapped_fd(cur, (int32_t)fd);
    int pipe_id = 0;
    int is_write_end = 0;
    if (decode_pipe_fd(mapped, &pipe_id, &is_write_end)) {
        if (is_write_end) {
            return -1;
        }
        struct nm_pipe *pipe = &pipe_table[pipe_id];
        if (!pipe->used) {
            return -1;
        }

        uint8_t *out = (uint8_t *)(uintptr_t)buf;
        uint64_t count = 0;
        while (count < len && pipe->read_pos < pipe->write_pos) {
            out[count] = pipe->buf[pipe->read_pos % NM_PIPE_BUF];
            pipe->read_pos++;
            count++;
        }
        return (int64_t)count;
    }

    if (mapped >= 0) {
        return fs_read(mapped, (void *)(uintptr_t)buf, len);
    }

    return -1;
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
        return -1;
    }

    int32_t value = cur->fd_table[fd];
    if (value == -1) {
        return -1;
    }

    if (close_task_fd_value(value) != 0) {
        return -1;
    }
    cur->fd_table[fd] = -1;
    return 0;
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
        return -1;
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
        return -1;
    }

    int pipe_id = -1;
    for (int i = 0; i < NM_PIPE_MAX; i++) {
        if (!pipe_table[i].used) {
            pipe_id = i;
            break;
        }
    }
    if (pipe_id < 0) {
        return -1;
    }

    int rd = alloc_task_fd(cur);
    if (rd < 0) {
        return -1;
    }
    cur->fd_table[rd] = -2;

    int wr = alloc_task_fd(cur);
    if (wr < 0) {
        cur->fd_table[rd] = -1;
        return -1;
    }

    pipe_table[pipe_id].used = 1;
    pipe_table[pipe_id].read_pos = 0;
    pipe_table[pipe_id].write_pos = 0;
    pipe_table[pipe_id].readers = 1;
    pipe_table[pipe_id].writers = 1;

    cur->fd_table[rd] = encode_pipe_fd(pipe_id, 0);
    cur->fd_table[wr] = encode_pipe_fd(pipe_id, 1);

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
        return -1;
    }

    int32_t oldv = cur->fd_table[oldfd];
    if (oldv == -1) {
        return -1;
    }

    if (oldfd == newfd) {
        return (int64_t)newfd;
    }

    int32_t newv = cur->fd_table[newfd];
    if (newv != -1 && close_task_fd_value(newv) != 0) {
        return -1;
    }

    cur->fd_table[newfd] = oldv;

    int pipe_id = 0;
    int is_write_end = 0;
    if (decode_pipe_fd(oldv, &pipe_id, &is_write_end)) {
        if (is_write_end) {
            pipe_table[pipe_id].writers++;
        } else {
            pipe_table[pipe_id].readers++;
        }
    }
    return (int64_t)newfd;
}

void syscall_init(void)
{
    for (size_t i = 0; i < NM_SYSCALL_MAX; i++) {
        syscall_table[i] = 0;
    }

    for (size_t i = 0; i < NM_PIPE_MAX; i++) {
        pipe_table[i].used = 0;
        pipe_table[i].read_pos = 0;
        pipe_table[i].write_pos = 0;
        pipe_table[i].readers = 0;
        pipe_table[i].writers = 0;
    }

    (void)syscall_register(NM_SYS_GETPID, sys_getpid);
    (void)syscall_register(NM_SYS_WRITE, sys_write);
    (void)syscall_register(NM_SYS_READ, sys_read);
    (void)syscall_register(NM_SYS_CLOSE, sys_close);
    (void)syscall_register(NM_SYS_EXIT, sys_exit);
    (void)syscall_register(NM_SYS_WAITPID, sys_waitpid);
    (void)syscall_register(NM_SYS_PIPE, sys_pipe);
    (void)syscall_register(NM_SYS_DUP2, sys_dup2);
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
