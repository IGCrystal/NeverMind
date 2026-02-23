#ifndef NM_FS_H
#define NM_FS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NM_PATH_MAX 256
#define NM_NAME_MAX 64
#define NM_FD_MAX 64

enum nm_node_type {
    NM_NODE_NONE = 0,
    NM_NODE_FILE,
    NM_NODE_DIR,
};

struct nm_stat {
    uint64_t ino;
    uint64_t size;
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t nlink;
    uint64_t blocks;
};

struct nm_vnode;

struct nm_file {
    bool used;
    uint32_t flags;
    uint64_t offset;
    struct nm_vnode *vnode;
};

struct nm_file_ops {
    int (*open)(struct nm_vnode *node, uint32_t flags);
    int64_t (*read)(struct nm_vnode *node, void *buf, uint64_t offset, uint64_t len);
    int64_t (*write)(struct nm_vnode *node, const void *buf, uint64_t offset, uint64_t len);
    int (*stat)(struct nm_vnode *node, struct nm_stat *st);
};

struct nm_vnode {
    bool used;
    uint64_t ino;
    enum nm_node_type type;
    char name[NM_NAME_MAX];
    uint32_t uid;
    uint32_t gid;
    uint32_t mode;
    struct nm_vnode *parent;
    struct nm_vnode *children;
    struct nm_vnode *next_sibling;
    uint8_t *data;
    uint64_t size;
    uint64_t capacity;
    const struct nm_file_ops *ops;
    void *fs_private;
};

struct nm_fs_ops {
    struct nm_vnode *(*mount_root)(void);
    struct nm_vnode *(*lookup)(struct nm_vnode *dir, const char *name);
    struct nm_vnode *(*create)(struct nm_vnode *dir, const char *name, enum nm_node_type type,
                               uint32_t mode);
};

struct nm_filesystem {
    const char *name;
    const struct nm_fs_ops *ops;
};

enum {
    NM_O_RDONLY = 0,
    NM_O_WRONLY = 1,
    NM_O_RDWR = 2,
    NM_O_CREAT = 0x40,
    NM_O_TRUNC = 0x200,
};

enum {
    NM_SEEK_SET = 0,
    NM_SEEK_CUR = 1,
    NM_SEEK_END = 2,
};

void fs_init(void);
int fs_mount_root(const struct nm_filesystem *fs);

int fs_open(const char *path, uint32_t flags, uint32_t mode);
int64_t fs_read(int fd, void *buf, uint64_t len);
int64_t fs_write(int fd, const void *buf, uint64_t len);
int64_t fs_lseek(int fd, int64_t offset, int whence);
int fs_close(int fd);
int fs_stat(const char *path, struct nm_stat *st);

int fs_path_split(const char *path, char out_parts[][NM_NAME_MAX], int max_parts);

const struct nm_filesystem *tmpfs_filesystem(void);
const struct nm_filesystem *ext2_filesystem(void);

#endif
