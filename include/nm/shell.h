#ifndef NM_SHELL_H
#define NM_SHELL_H

#include <stddef.h>

void shell_init(void);
int shell_execute_line(const char *line, char *out, size_t out_cap);
int shell_run_script(const char *script, char *out, size_t out_cap);

#endif
