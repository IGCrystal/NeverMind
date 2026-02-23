#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "nm/mm.h"

static void test_pmm_alloc_free(void)
{
    const struct nm_mem_range ranges[] = {
        {.base = 0x00000000, .length = 0x0009F000, .type = NM_MEM_AVAILABLE},
        {.base = 0x00100000, .length = 0x04000000, .type = NM_MEM_AVAILABLE},
    };

    pmm_init_from_ranges(ranges, sizeof(ranges) / sizeof(ranges[0]));
    struct nm_mm_stats before = pmm_get_stats();

    uint64_t p1 = pmm_alloc_page();
    uint64_t p2 = pmm_alloc_page();
    assert(p1 != 0);
    assert(p2 != 0);
    assert(p1 != p2);

    struct nm_mm_stats mid = pmm_get_stats();
    assert(mid.free_frames + 2 == before.free_frames);

    pmm_free_page(p1);
    pmm_free_page(p2);

    struct nm_mm_stats after = pmm_get_stats();
    assert(after.free_frames == before.free_frames);
}

static void test_kmalloc_reuse(void)
{
    const struct nm_mem_range ranges[] = {
        {.base = 0x00100000, .length = 0x02000000, .type = NM_MEM_AVAILABLE},
    };

    pmm_init_from_ranges(ranges, sizeof(ranges) / sizeof(ranges[0]));

    void *a = kmalloc(64);
    assert(a != 0);

    void *b = kmalloc(128);
    assert(b != 0);

    kfree(a);
    void *c = kmalloc(32);
    assert(c != 0);

    kfree(b);
    kfree(c);
}

int main(void)
{
    test_pmm_alloc_free();
    test_kmalloc_reuse();
    puts("test_pmm_kheap: PASS");
    return 0;
}
