#include "nm/proc.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/mm.h"

#define NM_MAX_TASKS 128
#define KSTACK_SIZE 8192

static struct nm_task task_table[NM_MAX_TASKS];
static size_t task_used;
static int32_t next_pid = 1;
static struct nm_task *current_task;
static volatile uint32_t proc_lock_word;

#ifndef NEVERMIND_HOST_TEST
extern void nm_kthread_trampoline(void);
#endif

#ifdef NEVERMIND_HOST_TEST
static uint8_t host_stacks[NM_MAX_TASKS][KSTACK_SIZE];
static size_t host_stack_cursor;
#endif

static void copy_name(char *dst, const char *src, size_t max_len)
{
    if (src == 0) {
        dst[0] = '\0';
        return;
    }

    size_t i = 0;
    for (; i + 1 < max_len && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static inline void proc_lock(void)
{
    while (__sync_lock_test_and_set(&proc_lock_word, 1U) != 0U) {
        __asm__ volatile("pause");
    }
}

static inline void proc_unlock(void)
{
    __sync_lock_release(&proc_lock_word);
}

static struct nm_task *alloc_task_slot(void)
{
    for (size_t i = 0; i < NM_MAX_TASKS; i++) {
        if (task_table[i].state == NM_TASK_UNUSED) {
            return &task_table[i];
        }
    }
    return 0;
}

static uint32_t count_ptr_vector(const char *const *vec)
{
    if (vec == 0) {
        return 0;
    }
    uint32_t count = 0;
    while (count < 64 && vec[count] != 0) {
        count++;
    }
    return count;
}

static uint8_t *alloc_task_stack(void)
{
#ifdef NEVERMIND_HOST_TEST
    if (host_stack_cursor >= NM_MAX_TASKS) {
        return 0;
    }
    return host_stacks[host_stack_cursor++];
#else
    return (uint8_t *)kmalloc(KSTACK_SIZE);
#endif
}

void proc_init(void)
{
    proc_lock_word = 0;
    proc_lock();
    for (size_t i = 0; i < NM_MAX_TASKS; i++) {
        task_table[i].state = NM_TASK_UNUSED;
        task_table[i].pid = 0;
        task_table[i].fd_cloexec_mask = 0;
        task_table[i].exit_code = 0;
        task_table[i].argc = 0;
        task_table[i].envc = 0;
        task_table[i].sched.rr_budget = 0;
        task_table[i].saved_rsp = 0;
    }
    task_used = 0;
    next_pid = 1;
    current_task = 0;
#ifdef NEVERMIND_HOST_TEST
    host_stack_cursor = 0;
#endif

    struct nm_task *bootstrap = alloc_task_slot();
    if (bootstrap == 0) {
        return;
    }

    bootstrap->pid = next_pid++;
    bootstrap->ppid = 0;
    bootstrap->is_kernel_thread = true;
    bootstrap->state = NM_TASK_RUNNING;
    bootstrap->sched.priority = 20;
    bootstrap->sched.timeslice_ticks = 4;
    bootstrap->sched.vruntime = 0;
    bootstrap->sched.rr_budget = 0;
    bootstrap->fd_cloexec_mask = 0;
    bootstrap->exit_code = 0;
    bootstrap->argc = 0;
    bootstrap->envc = 0;
    bootstrap->saved_rsp = 0;
    copy_name(bootstrap->name, "bootstrap", NM_TASK_NAME_MAX);
    for (size_t i = 0; i < NM_MAX_FDS; i++) {
        bootstrap->fd_table[i] = -1;
    }
    task_used = 1;
    current_task = bootstrap;
    proc_unlock();
}

struct nm_task *task_create_kernel_thread(const char *name, void (*entry)(void *), void *arg)
{
    uint8_t *kstack = alloc_task_stack();
    if (kstack == 0) {
        return 0;
    }

    proc_lock();
    struct nm_task *task = alloc_task_slot();
    if (task == 0) {
        proc_unlock();
#ifndef NEVERMIND_HOST_TEST
        kfree(kstack);
#endif
        return 0;
    }

    task->pid = next_pid++;
    task->ppid = current_task ? current_task->pid : 0;
    task->is_kernel_thread = true;
    task->state = NM_TASK_RUNNABLE;
    task->sched.priority = 20;
    task->sched.timeslice_ticks = 4;
    task->sched.vruntime = 0;
    task->sched.rr_budget = 0;
    task->fd_cloexec_mask = 0;
    task->exit_code = 0;
    task->argc = 0;
    task->envc = 0;
    task->kernel_stack_top = (uint64_t *)(uintptr_t)(kstack + KSTACK_SIZE);

#ifndef NEVERMIND_HOST_TEST
    uint64_t *sp = task->kernel_stack_top;
    sp = (uint64_t *)((uintptr_t)sp & ~0xFULL);
    *(--sp) = (uint64_t)(uintptr_t)arg;
    *(--sp) = (uint64_t)(uintptr_t)entry;
    *(--sp) = (uint64_t)(uintptr_t)&nm_kthread_trampoline;
    // nm_context_switch restores callee-saved registers from the new stack
    // before returning, so pre-populate them for a first-time switch-in.
    *(--sp) = 0; // rbp
    *(--sp) = 0; // rbx
    *(--sp) = 0; // r12
    *(--sp) = 0; // r13
    *(--sp) = 0; // r14
    *(--sp) = 0; // r15
    task->saved_rsp = sp;
#else
    (void)entry;
    (void)arg;
    task->saved_rsp = task->kernel_stack_top;
#endif

    task->regs.rsp = (uint64_t)(uintptr_t)task->kernel_stack_top;
    task->regs.rip = (uint64_t)(uintptr_t)entry;
    task->entry_name = name;
    copy_name(task->name, name, NM_TASK_NAME_MAX);

    for (size_t i = 0; i < NM_MAX_FDS; i++) {
        task->fd_table[i] = -1;
    }

    task_used++;
    proc_unlock();
    return task;
}

struct nm_task *task_current(void)
{
    proc_lock();
    struct nm_task *task = current_task;
    proc_unlock();
    return task;
}

struct nm_task *task_by_pid(int32_t pid)
{
    proc_lock();
    for (size_t i = 0; i < NM_MAX_TASKS; i++) {
        if (task_table[i].state != NM_TASK_UNUSED && task_table[i].pid == pid) {
            proc_unlock();
            return &task_table[i];
        }
    }
    proc_unlock();
    return 0;
}

struct nm_task *task_by_index(size_t index)
{
    proc_lock();
    if (index >= NM_MAX_TASKS) {
        proc_unlock();
        return 0;
    }
    if (task_table[index].state == NM_TASK_UNUSED) {
        proc_unlock();
        return 0;
    }
    struct nm_task *task = &task_table[index];
    proc_unlock();
    return task;
}

size_t task_count(void)
{
    proc_lock();
    size_t n = task_used;
    proc_unlock();
    return n;
}

void nm_set_current_task(struct nm_task *task)
{
    proc_lock();
    current_task = task;
    proc_unlock();
}

void proc_set_current(struct nm_task *task)
{
    proc_lock();
    current_task = task;
    proc_unlock();
}

struct nm_task *proc_fork_current(void)
{
    proc_lock();
    if (current_task == 0) {
        proc_unlock();
        return 0;
    }

    struct nm_task *child = alloc_task_slot();
    if (child == 0) {
        proc_unlock();
        return 0;
    }

    proc_unlock();

    uint8_t *kstack = alloc_task_stack();
    if (kstack == 0) {
        return 0;
    }

    proc_lock();
    if (current_task == 0 || child->state != NM_TASK_UNUSED) {
        proc_unlock();
#ifndef NEVERMIND_HOST_TEST
        kfree(kstack);
#endif
        return 0;
    }

    *child = *current_task;
    child->pid = next_pid++;
    child->ppid = current_task->pid;
    child->state = NM_TASK_RUNNABLE;
    child->exit_code = 0;
    child->sched.rr_budget = 0;
    child->kernel_stack_top = (uint64_t *)(uintptr_t)(kstack + KSTACK_SIZE);
    child->regs.rsp = (uint64_t)(uintptr_t)child->kernel_stack_top;
    child->regs.rax = 0;
    child->saved_rsp = child->kernel_stack_top;

    task_used++;
    proc_unlock();
    return child;
}

int proc_exec_current(const char *name, uint64_t entry, const char *const *argv,
                      const char *const *envp)
{
    proc_lock();
    if (current_task == 0 || name == 0) {
        proc_unlock();
        return -1;
    }

    current_task->entry_name = name;
    copy_name(current_task->name, name, NM_TASK_NAME_MAX);
    current_task->argc = count_ptr_vector(argv);
    current_task->envc = count_ptr_vector(envp);
    current_task->regs.rdi = (uint64_t)current_task->argc;
    current_task->regs.rsi = (uint64_t)(uintptr_t)argv;
    current_task->regs.rdx = (uint64_t)(uintptr_t)envp;
    if (entry != 0) {
        current_task->regs.rip = entry;
    }
    proc_unlock();
    return 0;
}

void proc_exit_current(int32_t code)
{
    proc_lock();
    if (current_task == 0) {
        proc_unlock();
        return;
    }
    current_task->exit_code = code;
    current_task->state = NM_TASK_ZOMBIE;
    proc_unlock();
}

int32_t proc_waitpid(int32_t pid, int32_t *status)
{
    proc_lock();
    if (current_task == 0) {
        proc_unlock();
        return -1;
    }

    struct nm_task *match = 0;
    for (size_t i = 0; i < NM_MAX_TASKS; i++) {
        struct nm_task *task = &task_table[i];
        if (task->state != NM_TASK_ZOMBIE) {
            continue;
        }
        if (task->ppid != current_task->pid) {
            continue;
        }
        if (pid > 0 && task->pid != pid) {
            continue;
        }
        match = task;
        break;
    }

    if (match == 0) {
        proc_unlock();
        return -1;
    }

    if (status != 0) {
        *status = match->exit_code;
    }

    int32_t found_pid = match->pid;
    match->state = NM_TASK_UNUSED;
    match->pid = 0;
    match->ppid = 0;
    match->fd_cloexec_mask = 0;
    match->exit_code = 0;
    if (task_used > 0) {
        task_used--;
    }
    proc_unlock();
    return found_pid;
}
