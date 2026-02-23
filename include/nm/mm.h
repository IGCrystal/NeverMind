#ifndef NM_MM_H
#define NM_MM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct nm_mem_range {
    uint64_t base;
    uint64_t length;
    uint32_t type;
};

struct nm_mm_stats {
    uint64_t total_frames;
    uint64_t free_frames;
    uint64_t used_frames;
    uint64_t reserved_frames;
};

enum {
    NM_MEM_AVAILABLE = 1,
};

void mm_init(uint64_t mb2_info_ptr);

void pmm_init_from_multiboot2(uint64_t mb2_info_ptr);
void pmm_init_from_ranges(const struct nm_mem_range *ranges, size_t count);
uint64_t pmm_alloc_page(void);
void pmm_free_page(uint64_t phys_addr);
struct nm_mm_stats pmm_get_stats(void);

#ifdef NEVERMIND_HOST_TEST
void *pmm_host_ptr_from_key(uint64_t key);
#endif

void vmm_init(void);
bool vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);
bool vmm_unmap_page(uint64_t virt_addr);
bool vmm_map_2m(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

void *kmalloc(size_t size);
void kfree(void *ptr);

#endif
