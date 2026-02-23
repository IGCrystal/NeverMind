#include <assert.h>
#include <stdio.h>

#include "nm/fs.h"
#include "nm/shell.h"

int main(void)
{
    fs_init();
    assert(fs_mount_root(tmpfs_filesystem()) == 0);
    shell_init();

    const char *script =
        "echo boot-line > /motd\n"
        "cat /motd\n"
        "echo x | cat\n";

    char out[512] = {0};
    assert(shell_run_script(script, out, sizeof(out)) == 0);
    assert(out[0] != '\0');

    struct nm_stat st;
    assert(fs_stat("/motd", &st) == 0);
    assert(st.size > 0);
    puts("test_boot_shell: PASS");
    return 0;
}
