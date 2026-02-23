#include "nm/exec.h"

#include <stddef.h>

struct nm_builtin_prog {
    const char *path;
    uint64_t entry;
};

static const struct nm_builtin_prog builtin_programs[] = {
    {"/init", 0x1000},
    {"/sh", 0x1100},
    {"/ping", 0x1200},
    {0, 0},
};

static int str_eq(const char *a, const char *b)
{
    size_t i = 0;
    if (a == 0 || b == 0) {
        return 0;
    }

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
}

int nm_exec_resolve_entry(const char *path, uint64_t *entry_out)
{
    if (path == 0 || entry_out == 0) {
        return -1;
    }

    for (size_t i = 0; builtin_programs[i].path != 0; i++) {
        if (str_eq(path, builtin_programs[i].path)) {
            *entry_out = builtin_programs[i].entry;
            return 0;
        }
    }
    return -1;
}
