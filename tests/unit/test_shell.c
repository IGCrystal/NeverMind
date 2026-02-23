#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "nm/fs.h"
#include "nm/shell.h"

static void test_echo_cat_redirect(void)
{
    fs_init();
    assert(fs_mount_root(tmpfs_filesystem()) == 0);
    shell_init();

    char out[256];
    assert(shell_execute_line("echo hello world > /hello.txt", out, sizeof(out)) == 0);

    assert(shell_execute_line("cat /hello.txt", out, sizeof(out)) == 0);
    assert(out[0] == 'h');
}

static void test_pipe(void)
{
    fs_init();
    assert(fs_mount_root(tmpfs_filesystem()) == 0);
    shell_init();

    char out[256];
    assert(shell_execute_line("echo pipe-ok | cat", out, sizeof(out)) == 0);
    assert(out[0] == 'p');
}

int main(void)
{
    test_echo_cat_redirect();
    test_pipe();
    puts("test_shell: PASS");
    return 0;
}
