#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "nm/proc.h"

static void kthread_stub(void *arg)
{
    (void)arg;
}

static void test_rr_pick(void)
{
    proc_init();
    struct nm_task *a = task_create_kernel_thread("a", kthread_stub, 0);
    struct nm_task *b = task_create_kernel_thread("b", kthread_stub, 0);
    assert(a != 0);
    assert(b != 0);

    sched_init(NM_SCHED_RR);
    struct nm_task *p1 = sched_pick_next();
    struct nm_task *p2 = sched_pick_next();
    assert(p1 != 0);
    assert(p2 != 0);
    assert(p1 != p2);
}

static void test_cfs_pick(void)
{
    proc_init();
    struct nm_task *slow = task_create_kernel_thread("slow", kthread_stub, 0);
    struct nm_task *fast = task_create_kernel_thread("fast", kthread_stub, 0);
    assert(slow != 0);
    assert(fast != 0);

    slow->sched.vruntime = 5000;
    fast->sched.vruntime = 100;

    sched_init(NM_SCHED_CFS);
    struct nm_task *pick = sched_pick_next();
    assert(pick == fast);

    sched_on_run(fast, 500);
    pick = sched_pick_next();
    assert(pick != 0);
}

int main(void)
{
    test_rr_pick();
    test_cfs_pick();
    puts("test_sched: PASS");
    return 0;
}
