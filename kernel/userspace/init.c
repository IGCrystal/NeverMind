#include "nm/userspace.h"

#include <stddef.h>

#include "nm/console.h"
#include "nm/shell.h"

void userspace_init(void)
{
    shell_init();
    console_write("[00.001100] userspace shell ready\n");

    static const char *boot_script =
        "echo NeverMind shell booted > /motd\n"
        "cat /motd\n";

    char out[512];
    if (shell_run_script(boot_script, out, sizeof(out)) == 0 && out[0] != '\0') {
        console_write(out);
    }
}
