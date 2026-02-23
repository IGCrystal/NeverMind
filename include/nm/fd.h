#ifndef NM_FD_H
#define NM_FD_H

#include <stdint.h>

#include "nm/proc.h"

void nm_fd_init(void);
int64_t nm_fd_read(struct nm_task *task, int32_t fd, void *buf, uint64_t len);
int64_t nm_fd_write(struct nm_task *task, int32_t fd, const void *buf, uint64_t len);
int nm_fd_close(struct nm_task *task, int32_t fd);
int nm_fd_pipe(struct nm_task *task, int32_t *read_fd, int32_t *write_fd);
int64_t nm_fd_dup2(struct nm_task *task, int32_t oldfd, int32_t newfd);
int nm_fd_on_fork_child(struct nm_task *child);
int nm_fd_set_cloexec(struct nm_task *task, int32_t fd, int enabled);
int nm_fd_get_cloexec(const struct nm_task *task, int32_t fd);
void nm_fd_close_on_exec(struct nm_task *task);

#endif
