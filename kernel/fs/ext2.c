#include "nm/fs.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/errno.h"

#ifdef NEVERMIND_HOST_TEST
#include <stdlib.h>
#endif

#define EXT2_MAX_INODES 1024

struct ext2_inode_meta {
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
    uint32_t links;
    uint32_t blocks;
};

static struct ext2_inode_meta inode_meta[EXT2_MAX_INODES];
static struct nm_vnode ext2_root;

// cppcheck-suppress constParameterCallback
static int ext2_open(struct nm_vnode *node, uint32_t flags)
{
    (void)flags;
    return node ? 0 : -1;
}

static int64_t ext2_read(struct nm_vnode *node, void *buf, uint64_t offset, uint64_t len)
{
    if (node == 0 || buf == 0 || node->data == 0 || offset >= node->size) {
        return 0;
    }
    uint64_t remain = node->size - offset;
    uint64_t n = (len < remain) ? len : remain;
    uint8_t *dst = (uint8_t *)buf;
    for (uint64_t i = 0; i < n; i++) {
        dst[i] = node->data[offset + i];
    }
    return (int64_t)n;
}

static int64_t ext2_write(struct nm_vnode *node, const void *buf, uint64_t offset, uint64_t len)
{
    if (node == 0 || buf == 0 || node->data == 0) {
        return NM_ERR(NM_EFAIL);
    }
    if (offset + len > node->capacity) {
        return NM_ERR(NM_EFAIL);
    }
    const uint8_t *src = (const uint8_t *)buf;
    for (uint64_t i = 0; i < len; i++) {
        node->data[offset + i] = src[i];
    }
    if (offset + len > node->size) {
        node->size = offset + len;
    }

    if (node->ino < EXT2_MAX_INODES) {
        inode_meta[node->ino].blocks = (uint32_t)((node->size + 511) / 512);
    }
    return (int64_t)len;
}

// cppcheck-suppress constParameterCallback
static int ext2_stat(struct nm_vnode *node, struct nm_stat *st)
{
    if (node == 0 || st == 0 || node->ino >= EXT2_MAX_INODES) {
        return NM_ERR(NM_EFAIL);
    }
    st->ino = node->ino;
    st->size = node->size;
    st->mode = inode_meta[node->ino].mode;
    st->uid = inode_meta[node->ino].uid;
    st->gid = inode_meta[node->ino].gid;
    st->nlink = inode_meta[node->ino].links;
    st->blocks = inode_meta[node->ino].blocks;
    return 0;
}

static const struct nm_file_ops ext2_file_ops = {
    .open = ext2_open,
    .read = ext2_read,
    .write = ext2_write,
    .stat = ext2_stat,
};

static struct nm_vnode *ext2_mount_root(void)
{
    ext2_root.used = true;
    ext2_root.ino = 2;
    ext2_root.type = NM_NODE_DIR;
    ext2_root.mode = 0755;
    ext2_root.uid = 0;
    ext2_root.gid = 0;
    ext2_root.parent = 0;
    ext2_root.children = 0;
    ext2_root.next_sibling = 0;
    ext2_root.data = 0;
    ext2_root.size = 0;
    ext2_root.capacity = 0;
    ext2_root.ops = &ext2_file_ops;
    ext2_root.name[0] = '/';
    ext2_root.name[1] = '\0';

    inode_meta[2].mode = 0755;
    inode_meta[2].uid = 0;
    inode_meta[2].gid = 0;
    inode_meta[2].links = 1;
    inode_meta[2].blocks = 0;

    return &ext2_root;
}

// cppcheck-suppress constParameterCallback
static struct nm_vnode *ext2_lookup(struct nm_vnode *dir, const char *name)
{
    if (dir == 0 || name == 0) {
        return 0;
    }
    struct nm_vnode *cur = dir->children;
    while (cur) {
        size_t i = 0;
        while (cur->name[i] != '\0' && name[i] != '\0' && cur->name[i] == name[i]) {
            i++;
        }
        if (cur->name[i] == '\0' && name[i] == '\0') {
            return cur;
        }
        cur = cur->next_sibling;
    }
    return 0;
}

static struct nm_vnode ext2_nodes[256];
static uint32_t ext2_node_cursor;

static struct nm_vnode *ext2_create(struct nm_vnode *dir, const char *name, enum nm_node_type type,
                                    uint32_t mode)
{
    if (dir == 0 || dir->type != NM_NODE_DIR || name == 0) {
        return 0;
    }
    if (ext2_node_cursor >= 256) {
        return 0;
    }
    if (ext2_lookup(dir, name) != 0) {
        return 0;
    }

    struct nm_vnode *node = &ext2_nodes[ext2_node_cursor++];
    node->used = true;
    node->ino = 12 + ext2_node_cursor;
    node->type = type;
    node->mode = mode;
    node->uid = 0;
    node->gid = 0;
    node->parent = dir;
    node->children = 0;
    node->next_sibling = dir->children;
    node->ops = &ext2_file_ops;
    node->size = 0;
    node->capacity = 4096;
    node->data = 0;
    node->fs_private = 0;

    for (size_t i = 0; i + 1 < NM_NAME_MAX && name[i] != '\0'; i++) {
        node->name[i] = name[i];
        node->name[i + 1] = '\0';
    }

#ifdef NEVERMIND_HOST_TEST
    node->data = (uint8_t *)malloc((size_t)node->capacity);
#else
    node->data = 0;
#endif

    if (node->ino < EXT2_MAX_INODES) {
        inode_meta[node->ino].mode = mode;
        inode_meta[node->ino].uid = 0;
        inode_meta[node->ino].gid = 0;
        inode_meta[node->ino].links = 1;
        inode_meta[node->ino].blocks = 0;
    }

    dir->children = node;
    return node;
}

static const struct nm_fs_ops ext2_ops = {
    .mount_root = ext2_mount_root,
    .lookup = ext2_lookup,
    .create = ext2_create,
};

static const struct nm_filesystem ext2_fs = {
    .name = "ext2",
    .ops = &ext2_ops,
};

const struct nm_filesystem *ext2_filesystem(void)
{
    return &ext2_fs;
}
