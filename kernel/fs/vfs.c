#include "nm/fs.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/errno.h"

static struct nm_file fd_table[NM_FD_MAX];
static const struct nm_filesystem *root_fs;
static struct nm_vnode *root_vnode;
static volatile uint32_t fs_lock_word;

static inline void fs_lock(void)
{
    while (__sync_lock_test_and_set(&fs_lock_word, 1U) != 0U) {
        __asm__ volatile("pause");
    }
}

static inline void fs_unlock(void)
{
    __sync_lock_release(&fs_lock_word);
}

static void copy_name(char *dst, const char *src)
{
    size_t i = 0;
    if (src == 0) {
        dst[0] = '\0';
        return;
    }
    for (; i + 1 < NM_NAME_MAX && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

int fs_path_split(const char *path, char out_parts[][NM_NAME_MAX], int max_parts)
{
    if (path == 0 || out_parts == 0 || max_parts <= 0) {
        return NM_ERR(NM_EFAIL);
    }

    int count = 0;
    size_t i = 0;
    while (path[i] != '\0') {
        while (path[i] == '/') {
            i++;
        }
        if (path[i] == '\0') {
            break;
        }
        if (count >= max_parts) {
            return NM_ERR(NM_EFAIL);
        }

        size_t j = 0;
        while (path[i] != '\0' && path[i] != '/' && j + 1 < NM_NAME_MAX) {
            out_parts[count][j++] = path[i++];
        }
        out_parts[count][j] = '\0';

        while (path[i] != '\0' && path[i] != '/') {
            i++;
        }
        count++;
    }
    return count;
}

static struct nm_vnode *resolve_path(const char *path, bool parent_only, char *leaf_name)
{
    fs_lock();
    const struct nm_filesystem *fs = root_fs;
    struct nm_vnode *root = root_vnode;
    fs_unlock();

    if (root == 0 || fs == 0 || path == 0 || path[0] != '/') {
        return 0;
    }

    char parts[16][NM_NAME_MAX];
    int part_count = fs_path_split(path, parts, 16);
    if (part_count < 0) {
        return 0;
    }
    if (part_count == 0) {
        if (leaf_name) {
            leaf_name[0] = '\0';
        }
        return root;
    }

    int limit = parent_only ? part_count - 1 : part_count;
    struct nm_vnode *cur = root;
    for (int i = 0; i < limit; i++) {
        cur = fs->ops->lookup(cur, parts[i]);
        if (cur == 0) {
            return 0;
        }
    }

    if (leaf_name) {
        copy_name(leaf_name, parts[part_count - 1]);
    }
    return cur;
}

static int alloc_fd(void)
{
    for (int i = 0; i < NM_FD_MAX; i++) {
        if (!fd_table[i].used) {
            fd_table[i].used = true;
            fd_table[i].offset = 0;
            fd_table[i].flags = 0;
            fd_table[i].vnode = 0;
            return i;
        }
    }
    return NM_ERR(NM_EFAIL);
}

void fs_init(void)
{
    fs_lock_word = 0;
    fs_lock();
    root_fs = 0;
    root_vnode = 0;
    for (int i = 0; i < NM_FD_MAX; i++) {
        fd_table[i].used = false;
        fd_table[i].offset = 0;
        fd_table[i].flags = 0;
        fd_table[i].vnode = 0;
    }
    fs_unlock();
}

int fs_mount_root(const struct nm_filesystem *fs)
{
    if (fs == 0 || fs->ops == 0 || fs->ops->mount_root == 0) {
        return NM_ERR(NM_EFAIL);
    }

    struct nm_vnode *mounted_root = fs->ops->mount_root();
    if (mounted_root == 0) {
        return NM_ERR(NM_EFAIL);
    }

    fs_lock();
    root_fs = fs;
    root_vnode = mounted_root;
    fs_unlock();
    return 0;
}

int fs_open(const char *path, uint32_t flags, uint32_t mode)
{
    (void)mode;
    if (path == 0) {
        return NM_ERR(NM_EFAIL);
    }

    struct nm_vnode *node = resolve_path(path, false, 0);
    if (node == 0 && (flags & NM_O_CREAT)) {
        char leaf[NM_NAME_MAX];
        struct nm_vnode *dir = resolve_path(path, true, leaf);
        if (dir == 0 || root_fs == 0 || root_fs->ops->create == 0) {
            return NM_ERR(NM_EFAIL);
        }
        node = root_fs->ops->create(dir, leaf, NM_NODE_FILE, 0644);
    }
    if (node == 0 || node->ops == 0) {
        return NM_ERR(NM_EFAIL);
    }

    fs_lock();
    int fd = alloc_fd();
    if (fd < 0) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }
    fs_unlock();

    if (node->ops->open && node->ops->open(node, flags) != 0) {
        fs_lock();
        fd_table[fd].used = false;
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }

    fs_lock();
    fd_table[fd].vnode = node;
    fd_table[fd].flags = flags;
    fd_table[fd].offset = 0;
    fs_unlock();
    return fd;
}

int64_t fs_read(int fd, void *buf, uint64_t len)
{
    fs_lock();
    if (fd < 0 || fd >= NM_FD_MAX || !fd_table[fd].used || buf == 0) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }
    struct nm_file *file = &fd_table[fd];
    if (file->vnode == 0 || file->vnode->ops == 0 || file->vnode->ops->read == 0) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }

    int64_t n = file->vnode->ops->read(file->vnode, buf, file->offset, len);
    if (n > 0) {
        file->offset += (uint64_t)n;
    }
    fs_unlock();
    return n;
}

int64_t fs_write(int fd, const void *buf, uint64_t len)
{
    fs_lock();
    if (fd < 0 || fd >= NM_FD_MAX || !fd_table[fd].used || buf == 0) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }
    struct nm_file *file = &fd_table[fd];
    if (file->vnode == 0 || file->vnode->ops == 0 || file->vnode->ops->write == 0) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }

    int64_t n = file->vnode->ops->write(file->vnode, buf, file->offset, len);
    if (n > 0) {
        file->offset += (uint64_t)n;
    }
    fs_unlock();
    return n;
}

int64_t fs_lseek(int fd, int64_t offset, int whence)
{
    fs_lock();
    if (fd < 0 || fd >= NM_FD_MAX || !fd_table[fd].used) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }

    struct nm_file *file = &fd_table[fd];
    int64_t base = 0;
    if (whence == NM_SEEK_CUR) {
        base = (int64_t)file->offset;
    } else if (whence == NM_SEEK_END) {
        base = (int64_t)(file->vnode ? file->vnode->size : 0);
    } else if (whence != NM_SEEK_SET) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }

    int64_t next = base + offset;
    if (next < 0) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }
    file->offset = (uint64_t)next;
    fs_unlock();
    return next;
}

int fs_close(int fd)
{
    fs_lock();
    if (fd < 0 || fd >= NM_FD_MAX || !fd_table[fd].used) {
        fs_unlock();
        return NM_ERR(NM_EFAIL);
    }
    fd_table[fd].used = false;
    fd_table[fd].offset = 0;
    fd_table[fd].flags = 0;
    fd_table[fd].vnode = 0;
    fs_unlock();
    return 0;
}

int fs_stat(const char *path, struct nm_stat *st)
{
    if (st == 0 || path == 0) {
        return NM_ERR(NM_EFAIL);
    }
    struct nm_vnode *node = resolve_path(path, false, 0);
    if (node == 0 || node->ops == 0 || node->ops->stat == 0) {
        return NM_ERR(NM_EFAIL);
    }
    return node->ops->stat(node, st);
}
