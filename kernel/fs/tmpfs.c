#include "nm/fs.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/string.h"

#ifdef NEVERMIND_HOST_TEST
#include <stdlib.h>
#define NM_ALLOC(sz) malloc(sz)
#define NM_FREE(p) free(p)
#else
#include "nm/mm.h"
#define NM_ALLOC(sz) kmalloc(sz)
#define NM_FREE(p) ((void)(p))
#endif

#define TMPFS_MAX_NODES 512

static struct nm_vnode nodes[TMPFS_MAX_NODES];
static uint64_t next_ino = 2;
static struct nm_vnode *tmpfs_root;

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

static int name_eq(const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

static struct nm_vnode *alloc_node(void)
{
    for (size_t i = 0; i < TMPFS_MAX_NODES; i++) {
        if (!nodes[i].used) {
            nodes[i].used = true;
            nodes[i].children = 0;
            nodes[i].next_sibling = 0;
            nodes[i].parent = 0;
            nodes[i].data = 0;
            nodes[i].size = 0;
            nodes[i].capacity = 0;
            nodes[i].fs_private = 0;
            nodes[i].uid = 0;
            nodes[i].gid = 0;
            return &nodes[i];
        }
    }
    return 0;
}

static int tmpfs_open(struct nm_vnode *node, uint32_t flags)
{
    (void)flags;
    return node ? 0 : -1;
}

static int64_t tmpfs_read(struct nm_vnode *node, void *buf, uint64_t offset, uint64_t len)
{
    if (node == 0 || buf == 0 || node->type != NM_NODE_FILE) {
        return -1;
    }
    if (offset >= node->size) {
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

static int tmpfs_reserve(struct nm_vnode *node, uint64_t need)
{
    if (need <= node->capacity) {
        return 0;
    }
    uint64_t cap = node->capacity ? node->capacity : 64;
    while (cap < need) {
        cap *= 2;
    }

    uint8_t *new_buf = (uint8_t *)NM_ALLOC((size_t)cap);
    if (new_buf == 0) {
        return -1;
    }

    for (uint64_t i = 0; i < node->size; i++) {
        new_buf[i] = node->data ? node->data[i] : 0;
    }
    if (node->data) {
        NM_FREE(node->data);
    }
    node->data = new_buf;
    node->capacity = cap;
    return 0;
}

static int64_t tmpfs_write(struct nm_vnode *node, const void *buf, uint64_t offset, uint64_t len)
{
    if (node == 0 || buf == 0 || node->type != NM_NODE_FILE) {
        return -1;
    }
    if (tmpfs_reserve(node, offset + len) != 0) {
        return -1;
    }

    const uint8_t *src = (const uint8_t *)buf;
    for (uint64_t i = 0; i < len; i++) {
        node->data[offset + i] = src[i];
    }
    if (offset + len > node->size) {
        node->size = offset + len;
    }
    return (int64_t)len;
}

static int tmpfs_stat(struct nm_vnode *node, struct nm_stat *st)
{
    if (node == 0 || st == 0) {
        return -1;
    }
    st->ino = node->ino;
    st->size = node->size;
    st->mode = node->mode;
    st->uid = node->uid;
    st->gid = node->gid;
    st->nlink = 1;
    st->blocks = (node->size + 511) / 512;
    return 0;
}

static const struct nm_file_ops tmpfs_file_ops = {
    .open = tmpfs_open,
    .read = tmpfs_read,
    .write = tmpfs_write,
    .stat = tmpfs_stat,
};

static struct nm_vnode *tmpfs_mount_root(void)
{
    for (size_t i = 0; i < TMPFS_MAX_NODES; i++) {
        nodes[i].used = false;
    }
    next_ino = 2;

    tmpfs_root = alloc_node();
    if (tmpfs_root == 0) {
        return 0;
    }

    tmpfs_root->ino = 1;
    tmpfs_root->type = NM_NODE_DIR;
    tmpfs_root->mode = 0755;
    copy_name(tmpfs_root->name, "/");
    tmpfs_root->ops = &tmpfs_file_ops;
    return tmpfs_root;
}

static struct nm_vnode *tmpfs_lookup(struct nm_vnode *dir, const char *name)
{
    if (dir == 0 || dir->type != NM_NODE_DIR || name == 0) {
        return 0;
    }
    struct nm_vnode *cur = dir->children;
    while (cur) {
        if (name_eq(cur->name, name)) {
            return cur;
        }
        cur = cur->next_sibling;
    }
    return 0;
}

static struct nm_vnode *tmpfs_create(struct nm_vnode *dir, const char *name, enum nm_node_type type,
                                     uint32_t mode)
{
    if (dir == 0 || dir->type != NM_NODE_DIR || name == 0) {
        return 0;
    }
    if (tmpfs_lookup(dir, name) != 0) {
        return 0;
    }

    struct nm_vnode *node = alloc_node();
    if (node == 0) {
        return 0;
    }

    node->ino = next_ino++;
    node->type = type;
    node->mode = mode;
    node->parent = dir;
    node->ops = &tmpfs_file_ops;
    copy_name(node->name, name);

    node->next_sibling = dir->children;
    dir->children = node;
    return node;
}

static const struct nm_fs_ops tmpfs_ops = {
    .mount_root = tmpfs_mount_root,
    .lookup = tmpfs_lookup,
    .create = tmpfs_create,
};

static const struct nm_filesystem tmpfs_fs = {
    .name = "tmpfs",
    .ops = &tmpfs_ops,
};

const struct nm_filesystem *tmpfs_filesystem(void)
{
    return &tmpfs_fs;
}
