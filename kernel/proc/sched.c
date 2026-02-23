#include "nm/proc.h"

#include <stddef.h>
#include <stdint.h>

extern void nm_set_current_task(struct nm_task *task);

static enum nm_sched_policy global_policy = NM_SCHED_RR;
static size_t rr_cursor;
static uint64_t rr_budget;

static uint32_t priority_weight(uint32_t priority)
{
    if (priority > 39) {
        priority = 39;
    }
    return 40U - priority;
}

void sched_init(enum nm_sched_policy policy)
{
    global_policy = policy;
    rr_cursor = 0;
    rr_budget = 0;
}

void sched_set_policy(enum nm_sched_policy policy)
{
    global_policy = policy;
}

enum nm_sched_policy sched_get_policy(void)
{
    return global_policy;
}

static struct nm_task *pick_rr(void)
{
    for (size_t probe = 0; probe < 128; probe++) {
        size_t idx = (rr_cursor + probe) % 128;
        struct nm_task *task = task_by_index(idx);
        if (task != 0 &&
            (task->state == NM_TASK_RUNNABLE || task->state == NM_TASK_RUNNING)) {
            rr_cursor = (idx + 1) % 128;
            return task;
        }
    }
    return task_current();
}

static struct nm_task *pick_cfs(void)
{
    struct nm_task *best = 0;

    for (size_t idx = 0; idx < 128; idx++) {
        struct nm_task *task = task_by_index(idx);
        if (task == 0) {
            continue;
        }
        if (task->state != NM_TASK_RUNNABLE) {
            continue;
        }

        if (best == 0 || task->sched.vruntime < best->sched.vruntime) {
            best = task;
        }
    }

    if (best != 0) {
        return best;
    }

    for (size_t idx = 0; idx < 128; idx++) {
        struct nm_task *task = task_by_index(idx);
        if (task == 0) {
            continue;
        }
        if (task->state != NM_TASK_RUNNING) {
            continue;
        }

        if (best == 0 || task->sched.vruntime < best->sched.vruntime) {
            best = task;
        }
    }

    if (best == 0) {
        best = task_current();
    }
    return best;
}

struct nm_task *sched_pick_next(void)
{
    if (global_policy == NM_SCHED_CFS) {
        return pick_cfs();
    }
    return pick_rr();
}

void sched_on_run(struct nm_task *task, uint64_t ticks)
{
    if (task == 0 || ticks == 0) {
        return;
    }

    if (global_policy == NM_SCHED_CFS) {
        uint32_t w = priority_weight(task->sched.priority);
        task->sched.vruntime += (ticks * 1024ULL) / (uint64_t)w;
        return;
    }

    rr_budget += ticks;
    if (rr_budget >= task->sched.timeslice_ticks) {
        rr_budget = 0;
        task->state = NM_TASK_RUNNABLE;
    }
}

void sched_tick(uint64_t ticks)
{
    struct nm_task *cur = task_current();
    if (cur == 0) {
        return;
    }

    sched_on_run(cur, ticks);
    struct nm_task *next = sched_pick_next();
    if (next == 0 || next == cur) {
        return;
    }

    cur->state = NM_TASK_RUNNABLE;
    next->state = NM_TASK_RUNNING;
    nm_set_current_task(next);
}
