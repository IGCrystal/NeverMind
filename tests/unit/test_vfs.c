#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "nm/fs.h"

static void test_path_split(void)
{
    char parts[8][NM_NAME_MAX];
    int n = fs_path_split("/a/bb/ccc", parts, 8);
    assert(n == 3);
    assert(parts[0][0] == 'a');
    assert(parts[1][0] == 'b');
    assert(parts[2][0] == 'c');
}

static void test_tmpfs_rw(void)
{
    fs_init();
    assert(fs_mount_root(tmpfs_filesystem()) == 0);

    int fd = fs_open("/hello.txt", NM_O_CREAT | NM_O_RDWR, 0644);
    assert(fd >= 0);

    const char *msg = "NeverMind";
    int64_t wn = fs_write(fd, msg, 9);
    assert(wn == 9);

    assert(fs_lseek(fd, 0, NM_SEEK_SET) == 0);
    char buf[16];
    int64_t rn = fs_read(fd, buf, 9);
    assert(rn == 9);
    assert(buf[0] == 'N');
    assert(buf[8] == 'd');
    assert(fs_close(fd) == 0);

    struct nm_stat st;
    assert(fs_stat("/hello.txt", &st) == 0);
    assert(st.size == 9);
}

static void test_ext2_create_and_stat(void)
{
    fs_init();
    assert(fs_mount_root(ext2_filesystem()) == 0);

    int fd = fs_open("/ext2-file", NM_O_CREAT | NM_O_RDWR, 0644);
    assert(fd >= 0);

    const char payload[] = {'A', 'B', 'C'};
    assert(fs_write(fd, payload, 3) == 3);
    assert(fs_close(fd) == 0);

    struct nm_stat st;
    assert(fs_stat("/ext2-file", &st) == 0);
    assert(st.size == 3);
}

int main(void)
{
    test_path_split();
    test_tmpfs_rw();
    test_ext2_create_and_stat();
    puts("test_vfs: PASS");
    return 0;
}
