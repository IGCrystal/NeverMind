#ifndef NM_PROC_H
#define NM_PROC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define NM_MAX_FDS 32
#define NM_TASK_NAME_MAX 24

enum nm_task_state {
    NM_TASK_UNUSED = 0,
    NM_TASK_RUNNABLE,
    NM_TASK_RUNNING,
    NM_TASK_SLEEPING,
    NM_TASK_ZOMBIE,
};

enum nm_sched_policy {
    NM_SCHED_RR = 0,
    NM_SCHED_CFS,
};

struct nm_regs {
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t rbx;
    uint64_t rax;
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
};

struct nm_sched_param {
    uint32_t priority;
    uint32_t timeslice_ticks;
    uint64_t vruntime;
};

struct nm_task {
    int32_t pid;
    int32_t ppid;
    bool is_kernel_thread;
    enum nm_task_state state;
    struct nm_regs regs;
    uint64_t cr3;
    int32_t fd_table[NM_MAX_FDS];
    uint32_t fd_cloexec_mask;
    uint64_t signal_mask;
    int32_t exit_code;
    uint32_t argc;
    uint32_t envc;
    struct nm_sched_param sched;
    uint64_t *kernel_stack_top;
    const char *entry_name;
    char name[NM_TASK_NAME_MAX];
};

void proc_init(void);
struct nm_task *task_create_kernel_thread(const char *name, void (*entry)(void *), void *arg);
struct nm_task *task_current(void);
struct nm_task *task_by_pid(int32_t pid);
struct nm_task *task_by_index(size_t index);
size_t task_count(void);
void proc_set_current(struct nm_task *task);
struct nm_task *proc_fork_current(void);
int proc_exec_current(const char *name, uint64_t entry, const char *const *argv,
                      const char *const *envp);
void proc_exit_current(int32_t code);
int32_t proc_waitpid(int32_t pid, int32_t *status);

void sched_init(enum nm_sched_policy policy);
void sched_set_policy(enum nm_sched_policy policy);
enum nm_sched_policy sched_get_policy(void);
void sched_tick(uint64_t ticks);
struct nm_task *sched_pick_next(void);
void sched_on_run(struct nm_task *task, uint64_t ticks);

void nm_context_switch(uint64_t **old_rsp, uint64_t *new_rsp);

#endif
