#ifndef NM_EXEC_H
#define NM_EXEC_H

#include <stdint.h>

int nm_exec_resolve_entry(const char *path, uint64_t *entry_out);

#endif
