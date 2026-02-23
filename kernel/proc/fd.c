#include "nm/fd.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/fs.h"

#define NM_PIPE_MAX 16
#define NM_PIPE_BUF 512
#define NM_FDOBJ_MAX 128

enum nm_fdobj_kind {
    NM_FDOBJ_NONE = 0,
    NM_FDOBJ_FS,
    NM_FDOBJ_PIPE_R,
    NM_FDOBJ_PIPE_W,
};

struct nm_pipe {
    int used;
    uint8_t buf[NM_PIPE_BUF];
    uint64_t read_pos;
    uint64_t write_pos;
    uint32_t readers;
    uint32_t writers;
};

struct nm_fdobj {
    int used;
    uint32_t refcnt;
    enum nm_fdobj_kind kind;
    int32_t fs_fd;
    int32_t pipe_id;
};

static struct nm_pipe pipe_table[NM_PIPE_MAX];
static struct nm_fdobj fdobj_table[NM_FDOBJ_MAX];

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

static int alloc_fdobj(void)
{
    for (int i = 0; i < NM_FDOBJ_MAX; i++) {
        if (!fdobj_table[i].used) {
            fdobj_table[i].used = 1;
            fdobj_table[i].refcnt = 0;
            fdobj_table[i].kind = NM_FDOBJ_NONE;
            fdobj_table[i].fs_fd = -1;
            fdobj_table[i].pipe_id = -1;
            return i;
        }
    }
    return -1;
}

static struct nm_fdobj *fdobj_get(int32_t id)
{
    if (id < 0 || id >= NM_FDOBJ_MAX) {
        return 0;
    }
    if (!fdobj_table[id].used) {
        return 0;
    }
    return &fdobj_table[id];
}

static void pipe_on_endpoint_acquire(enum nm_fdobj_kind kind, int32_t pipe_id)
{
    if (pipe_id < 0 || pipe_id >= NM_PIPE_MAX) {
        return;
    }
    struct nm_pipe *pipe = &pipe_table[pipe_id];
    if (!pipe->used) {
        return;
    }
    if (kind == NM_FDOBJ_PIPE_R) {
        pipe->readers++;
    } else if (kind == NM_FDOBJ_PIPE_W) {
        pipe->writers++;
    }
}

static void pipe_on_endpoint_release(enum nm_fdobj_kind kind, int32_t pipe_id)
{
    if (pipe_id < 0 || pipe_id >= NM_PIPE_MAX) {
        return;
    }
    struct nm_pipe *pipe = &pipe_table[pipe_id];
    if (!pipe->used) {
        return;
    }
    if (kind == NM_FDOBJ_PIPE_R) {
        if (pipe->readers > 0) {
            pipe->readers--;
        }
    } else if (kind == NM_FDOBJ_PIPE_W) {
        if (pipe->writers > 0) {
            pipe->writers--;
        }
    }

    if (pipe->readers == 0 && pipe->writers == 0) {
        pipe->used = 0;
        pipe->read_pos = 0;
        pipe->write_pos = 0;
    }
}

static int fdobj_retain(int32_t id)
{
    struct nm_fdobj *obj = fdobj_get(id);
    if (obj == 0) {
        return -1;
    }

    obj->refcnt++;
    pipe_on_endpoint_acquire(obj->kind, obj->pipe_id);
    return 0;
}

static int fdobj_release(int32_t id)
{
    struct nm_fdobj *obj = fdobj_get(id);
    if (obj == 0 || obj->refcnt == 0) {
        return -1;
    }

    pipe_on_endpoint_release(obj->kind, obj->pipe_id);

    obj->refcnt--;
    if (obj->refcnt > 0) {
        return 0;
    }

    if (obj->kind == NM_FDOBJ_FS && obj->fs_fd >= 0) {
        (void)fs_close(obj->fs_fd);
    }

    obj->used = 0;
    obj->kind = NM_FDOBJ_NONE;
    obj->fs_fd = -1;
    obj->pipe_id = -1;
    return 0;
}

static int32_t task_fdobj_id(struct nm_task *task, int32_t fd)
{
    if (task == 0 || fd < 0 || fd >= NM_MAX_FDS) {
        return -1;
    }
    return task->fd_table[fd];
}

static int task_install_fdobj(struct nm_task *task, int32_t fd, int32_t obj_id)
{
    if (task == 0 || fd < 0 || fd >= NM_MAX_FDS) {
        return -1;
    }
    task->fd_table[fd] = obj_id;
    return 0;
}

static int task_close_fd(struct nm_task *task, int32_t fd)
{
    if (task == 0 || fd < 0 || fd >= NM_MAX_FDS) {
        return -1;
    }

    int32_t obj_id = task->fd_table[fd];
    if (obj_id == -1) {
        return -1;
    }

    if (fdobj_release(obj_id) != 0) {
        return -1;
    }
    task->fd_table[fd] = -1;
    task->fd_cloexec_mask &= ~(1U << (uint32_t)fd);
    return 0;
}

static int create_pipe_endpoint_obj(enum nm_fdobj_kind kind, int32_t pipe_id)
{
    int obj_id = alloc_fdobj();
    if (obj_id < 0) {
        return -1;
    }

    struct nm_fdobj *obj = &fdobj_table[obj_id];
    obj->kind = kind;
    obj->pipe_id = pipe_id;
    obj->refcnt = 1;
    return obj_id;
}

static int create_fs_obj(int32_t fs_fd)
{
    int obj_id = alloc_fdobj();
    if (obj_id < 0) {
        return -1;
    }

    struct nm_fdobj *obj = &fdobj_table[obj_id];
    obj->kind = NM_FDOBJ_FS;
    obj->fs_fd = fs_fd;
    obj->refcnt = 1;
    return obj_id;
}

static int64_t read_from_pipe(struct nm_pipe *pipe, void *buf, uint64_t len)
{
    if (pipe == 0 || !pipe->used || buf == 0) {
        return -1;
    }

    uint8_t *out = (uint8_t *)buf;
    uint64_t count = 0;
    while (count < len && pipe->read_pos < pipe->write_pos) {
        out[count] = pipe->buf[pipe->read_pos % NM_PIPE_BUF];
        pipe->read_pos++;
        count++;
    }
    return (int64_t)count;
}

static int64_t write_to_pipe(struct nm_pipe *pipe, const void *buf, uint64_t len)
{
    if (pipe == 0 || !pipe->used || buf == 0) {
        return -1;
    }

    const uint8_t *in = (const uint8_t *)buf;
    uint64_t written = 0;
    while (written < len && (pipe->write_pos - pipe->read_pos) < NM_PIPE_BUF) {
        pipe->buf[pipe->write_pos % NM_PIPE_BUF] = in[written];
        pipe->write_pos++;
        written++;
    }
    return (int64_t)written;
}

static int64_t read_from_fdobj(struct nm_fdobj *obj, void *buf, uint64_t len)
{
    if (obj == 0) {
        return -1;
    }

    if (obj->kind == NM_FDOBJ_PIPE_R) {
        if (obj->pipe_id < 0 || obj->pipe_id >= NM_PIPE_MAX) {
            return -1;
        }
        return read_from_pipe(&pipe_table[obj->pipe_id], buf, len);
    }

    if (obj->kind == NM_FDOBJ_FS) {
        return fs_read(obj->fs_fd, buf, len);
    }

    return -1;
}

static int64_t write_to_fdobj(struct nm_fdobj *obj, const void *buf, uint64_t len)
{
    if (obj == 0) {
        return -1;
    }

    if (obj->kind == NM_FDOBJ_PIPE_W) {
        if (obj->pipe_id < 0 || obj->pipe_id >= NM_PIPE_MAX) {
            return -1;
        }
        return write_to_pipe(&pipe_table[obj->pipe_id], buf, len);
    }

    if (obj->kind == NM_FDOBJ_FS) {
        return fs_write(obj->fs_fd, buf, len);
    }

    return -1;
}

static int adopt_legacy_fs_fd(struct nm_task *task, int32_t fd)
{
    int32_t legacy = task_fdobj_id(task, fd);
    if (legacy < 0 || legacy >= NM_FDOBJ_MAX) {
        return -1;
    }

    struct nm_fdobj *obj = fdobj_get(legacy);
    if (obj != 0) {
        return legacy;
    }

    int obj_id = create_fs_obj(legacy);
    if (obj_id < 0) {
        return -1;
    }

    (void)task_install_fdobj(task, fd, obj_id);
    return obj_id;
}

void nm_fd_init(void)
{
    for (size_t i = 0; i < NM_PIPE_MAX; i++) {
        pipe_table[i].used = 0;
        pipe_table[i].read_pos = 0;
        pipe_table[i].write_pos = 0;
        pipe_table[i].readers = 0;
        pipe_table[i].writers = 0;
    }

    for (size_t i = 0; i < NM_FDOBJ_MAX; i++) {
        fdobj_table[i].used = 0;
        fdobj_table[i].refcnt = 0;
        fdobj_table[i].kind = NM_FDOBJ_NONE;
        fdobj_table[i].fs_fd = -1;
        fdobj_table[i].pipe_id = -1;
    }
}

int64_t nm_fd_read(struct nm_task *task, int32_t fd, void *buf, uint64_t len)
{
    if (task == 0 || buf == 0) {
        return -1;
    }

    int32_t obj_id = task_fdobj_id(task, fd);
    if (obj_id == -1) {
        return -1;
    }

    if (obj_id >= 0 && obj_id < NM_FDOBJ_MAX) {
        struct nm_fdobj *obj = fdobj_get(obj_id);
        if (obj != 0) {
            return read_from_fdobj(obj, buf, len);
        }
    }

    int adopted = adopt_legacy_fs_fd(task, fd);
    if (adopted >= 0) {
        struct nm_fdobj *obj = fdobj_get(adopted);
        if (obj != 0) {
            return read_from_fdobj(obj, buf, len);
        }
    }

    return -1;
}

int64_t nm_fd_write(struct nm_task *task, int32_t fd, const void *buf, uint64_t len)
{
    if (task == 0 || buf == 0) {
        return -1;
    }

    int32_t obj_id = task_fdobj_id(task, fd);
    if (obj_id == -1) {
        return -1;
    }

    if (obj_id >= 0 && obj_id < NM_FDOBJ_MAX) {
        struct nm_fdobj *obj = fdobj_get(obj_id);
        if (obj != 0) {
            return write_to_fdobj(obj, buf, len);
        }
    }

    int adopted = adopt_legacy_fs_fd(task, fd);
    if (adopted >= 0) {
        struct nm_fdobj *obj = fdobj_get(adopted);
        if (obj != 0) {
            return write_to_fdobj(obj, buf, len);
        }
    }

    return -1;
}

int nm_fd_close(struct nm_task *task, int32_t fd)
{
    if (task == 0 || fd < 0 || fd >= NM_MAX_FDS) {
        return -1;
    }

    int32_t obj_id = task_fdobj_id(task, fd);
    if (obj_id == -1) {
        return -1;
    }

    if (obj_id >= 0 && obj_id < NM_FDOBJ_MAX && fdobj_get(obj_id) == 0) {
        if (adopt_legacy_fs_fd(task, fd) < 0) {
            return -1;
        }
    }

    return task_close_fd(task, fd);
}

int nm_fd_pipe(struct nm_task *task, int32_t *read_fd, int32_t *write_fd)
{
    if (task == 0 || read_fd == 0 || write_fd == 0) {
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

    int rd = alloc_task_fd(task);
    if (rd < 0) {
        return -1;
    }
    task->fd_table[rd] = -2;

    int wr = alloc_task_fd(task);
    if (wr < 0) {
        task->fd_table[rd] = -1;
        return -1;
    }

    pipe_table[pipe_id].used = 1;
    pipe_table[pipe_id].read_pos = 0;
    pipe_table[pipe_id].write_pos = 0;
    pipe_table[pipe_id].readers = 1;
    pipe_table[pipe_id].writers = 1;

    int rd_obj = create_pipe_endpoint_obj(NM_FDOBJ_PIPE_R, pipe_id);
    if (rd_obj < 0) {
        task->fd_table[rd] = -1;
        pipe_table[pipe_id].used = 0;
        pipe_table[pipe_id].readers = 0;
        pipe_table[pipe_id].writers = 0;
        return -1;
    }

    int wr_obj = create_pipe_endpoint_obj(NM_FDOBJ_PIPE_W, pipe_id);
    if (wr_obj < 0) {
        task->fd_table[rd] = -1;
        (void)fdobj_release(rd_obj);
        pipe_table[pipe_id].used = 0;
        pipe_table[pipe_id].readers = 0;
        pipe_table[pipe_id].writers = 0;
        return -1;
    }

    (void)task_install_fdobj(task, rd, rd_obj);
    (void)task_install_fdobj(task, wr, wr_obj);

    *read_fd = rd;
    *write_fd = wr;
    return 0;
}

int64_t nm_fd_dup2(struct nm_task *task, int32_t oldfd, int32_t newfd)
{
    if (task == 0 || oldfd < 0 || newfd < 0 || oldfd >= NM_MAX_FDS || newfd >= NM_MAX_FDS) {
        return -1;
    }

    int32_t old_obj = task_fdobj_id(task, oldfd);
    if (old_obj == -1) {
        return -1;
    }

    if (oldfd == newfd) {
        return newfd;
    }

    int32_t new_obj = task_fdobj_id(task, newfd);
    if (new_obj != -1 && task_close_fd(task, newfd) != 0) {
        return -1;
    }

    if (fdobj_retain(old_obj) != 0) {
        return -1;
    }

    (void)task_install_fdobj(task, newfd, old_obj);
    task->fd_cloexec_mask &= ~(1U << (uint32_t)newfd);
    return newfd;
}

int nm_fd_on_fork_child(struct nm_task *child)
{
    if (child == 0) {
        return -1;
    }

    for (int i = 0; i < NM_MAX_FDS; i++) {
        int32_t obj_id = child->fd_table[i];
        if (obj_id != -1 && fdobj_retain(obj_id) != 0) {
            child->fd_table[i] = -1;
        }
    }
    return 0;
}

int nm_fd_set_cloexec(struct nm_task *task, int32_t fd, int enabled)
{
    if (task == 0 || fd < 0 || fd >= NM_MAX_FDS || task->fd_table[fd] == -1) {
        return -1;
    }

    uint32_t bit = 1U << (uint32_t)fd;
    if (enabled) {
        task->fd_cloexec_mask |= bit;
    } else {
        task->fd_cloexec_mask &= ~bit;
    }
    return 0;
}

int nm_fd_get_cloexec(struct nm_task *task, int32_t fd)
{
    if (task == 0 || fd < 0 || fd >= NM_MAX_FDS || task->fd_table[fd] == -1) {
        return -1;
    }

    uint32_t bit = 1U << (uint32_t)fd;
    return (task->fd_cloexec_mask & bit) ? 1 : 0;
}

void nm_fd_close_on_exec(struct nm_task *task)
{
    if (task == 0) {
        return;
    }

    for (int fd = 0; fd < NM_MAX_FDS; fd++) {
        uint32_t bit = 1U << (uint32_t)fd;
        if ((task->fd_cloexec_mask & bit) == 0) {
            continue;
        }
        (void)nm_fd_close(task, fd);
    }
}
