#include "nm/mm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096ULL
#define PAGE_FLAG_PRESENT 0x1ULL
#define PAGE_FLAG_RW 0x2ULL
#define PAGE_FLAG_US 0x4ULL
#define PAGE_FLAG_PS 0x80ULL
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL

static uint64_t *kernel_pml4;
static volatile uint32_t vmm_lock_word;

static inline uint64_t ptr_to_phys(const void *ptr)
{
    uint64_t addr = (uint64_t)(uintptr_t)ptr;
    if (addr >= KERNEL_VIRT_BASE) {
        return addr - KERNEL_VIRT_BASE;
    }
    return addr;
}

static inline uint64_t *phys_to_ptr(uint64_t phys)
{
    return (uint64_t *)(uintptr_t)phys;
}

static inline uint64_t table_flags_from_leaf(uint64_t flags)
{
    uint64_t out = 0;
    if ((flags & PAGE_FLAG_RW) != 0) {
        out |= PAGE_FLAG_RW;
    }
    if ((flags & PAGE_FLAG_US) != 0) {
        out |= PAGE_FLAG_US;
    }
    return out;
}

static inline void vmm_lock(void)
{
    while (__sync_lock_test_and_set(&vmm_lock_word, 1U) != 0U) {
        __asm__ volatile("pause");
    }
}

static inline void vmm_unlock(void)
{
    __sync_lock_release(&vmm_lock_word);
}

static bool table_empty(const uint64_t *table)
{
    for (size_t i = 0; i < 512; i++) {
        if ((table[i] & PAGE_FLAG_PRESENT) != 0) {
            return false;
        }
    }
    return true;
}

static uint64_t *alloc_table(void)
{
    uint64_t phys = pmm_alloc_page();
    if (phys == 0) {
        return 0;
    }

    uint64_t *table = phys_to_ptr(phys);
    for (int i = 0; i < 512; i++) {
        table[i] = 0;
    }
    return table;
}

static bool alloc_table_pool(uint64_t **pool, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        pool[i] = alloc_table();
        if (pool[i] == 0) {
            for (size_t j = 0; j < i; j++) {
                pmm_free_page(ptr_to_phys(pool[j]));
                pool[j] = 0;
            }
            return false;
        }
    }
    return true;
}

static void free_table_pool(uint64_t **pool, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (pool[i] != 0) {
            pmm_free_page(ptr_to_phys(pool[i]));
            pool[i] = 0;
        }
    }
}

static uint64_t *take_prealloc(uint64_t **pool, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        if (pool[i] != 0) {
            uint64_t *table = pool[i];
            pool[i] = 0;
            return table;
        }
    }
    return 0;
}

static uint64_t *get_or_create_next(uint64_t *table, uint16_t index, uint64_t flags,
                                    uint64_t **prealloc_pool, size_t prealloc_count)
{
    uint64_t entry = table[index];
    if ((entry & PAGE_FLAG_PRESENT) != 0) {
        uint64_t want = flags & (PAGE_FLAG_RW | PAGE_FLAG_US);
        if ((entry & want) != want) {
            table[index] = entry | want;
        }
        return phys_to_ptr(entry & ~0xFFFULL);
    }

    uint64_t *next = take_prealloc(prealloc_pool, prealloc_count);
    if (next == 0) {
        return 0;
    }

    table[index] = ptr_to_phys(next) | flags | PAGE_FLAG_PRESENT;
    return next;
}

void vmm_init(void)
{
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    kernel_pml4 = phys_to_ptr(cr3 & ~0xFFFULL);
}

uint64_t *vmm_kernel_root(void)
{
    return kernel_pml4;
}

bool vmm_map_page_in(uint64_t *pml4, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags)
{
    if (pml4 == 0) {
        return false;
    }

    uint64_t *prealloc[3] = {0, 0, 0};
    if (!alloc_table_pool(prealloc, 3)) {
        return false;
    }

    vmm_lock();

    uint16_t pml4_i = (virt_addr >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_i = (virt_addr >> 21) & 0x1FF;
    uint16_t pt_i = (virt_addr >> 12) & 0x1FF;
    uint64_t table_flags = table_flags_from_leaf(flags);

    uint64_t *pdpt = get_or_create_next(pml4, pml4_i, table_flags, prealloc, 3);
    if (pdpt == 0) {
        vmm_unlock();
        free_table_pool(prealloc, 3);
        return false;
    }

    uint64_t *pd = get_or_create_next(pdpt, pdpt_i, table_flags, prealloc, 3);
    if (pd == 0) {
        vmm_unlock();
        free_table_pool(prealloc, 3);
        return false;
    }

    uint64_t *pt = get_or_create_next(pd, pd_i, table_flags, prealloc, 3);
    if (pt == 0) {
        vmm_unlock();
        free_table_pool(prealloc, 3);
        return false;
    }

    pt[pt_i] = (phys_addr & ~0xFFFULL) | (flags & 0xFFFULL) | PAGE_FLAG_PRESENT;
    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)virt_addr) : "memory");
    vmm_unlock();
    free_table_pool(prealloc, 3);
    return true;
}

bool vmm_map_2m_in(uint64_t *pml4, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags)
{
    if (pml4 == 0) {
        return false;
    }

    uint64_t *prealloc[2] = {0, 0};
    if (!alloc_table_pool(prealloc, 2)) {
        return false;
    }

    vmm_lock();

    uint16_t pml4_i = (virt_addr >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_i = (virt_addr >> 21) & 0x1FF;
    uint64_t table_flags = table_flags_from_leaf(flags);

    uint64_t *pdpt = get_or_create_next(pml4, pml4_i, table_flags, prealloc, 2);
    if (pdpt == 0) {
        vmm_unlock();
        free_table_pool(prealloc, 2);
        return false;
    }

    uint64_t *pd = get_or_create_next(pdpt, pdpt_i, table_flags, prealloc, 2);
    if (pd == 0) {
        vmm_unlock();
        free_table_pool(prealloc, 2);
        return false;
    }

    pd[pd_i] = (phys_addr & ~0x1FFFFFULL) | (flags & 0xFFFULL) | PAGE_FLAG_PRESENT | PAGE_FLAG_PS;
    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)virt_addr) : "memory");
    vmm_unlock();
    free_table_pool(prealloc, 2);
    return true;
}

bool vmm_unmap_page_in(uint64_t *pml4, uint64_t virt_addr)
{
    if (pml4 == 0) {
        return false;
    }

    uint64_t to_free[3] = {0, 0, 0};
    size_t free_count = 0;

    vmm_lock();

    uint16_t pml4_i = (virt_addr >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_i = (virt_addr >> 21) & 0x1FF;
    uint16_t pt_i = (virt_addr >> 12) & 0x1FF;

    if ((pml4[pml4_i] & PAGE_FLAG_PRESENT) == 0) {
        vmm_unlock();
        return false;
    }

    uint64_t pml4e = pml4[pml4_i];
    uint64_t *pdpt = phys_to_ptr(pml4e & ~0xFFFULL);
    if ((pdpt[pdpt_i] & PAGE_FLAG_PRESENT) == 0) {
        vmm_unlock();
        return false;
    }

    uint64_t pdpte = pdpt[pdpt_i];
    uint64_t *pd = phys_to_ptr(pdpte & ~0xFFFULL);
    if ((pd[pd_i] & PAGE_FLAG_PRESENT) == 0 || (pd[pd_i] & PAGE_FLAG_PS) != 0) {
        vmm_unlock();
        return false;
    }

    uint64_t pde = pd[pd_i];
    uint64_t *pt = phys_to_ptr(pde & ~0xFFFULL);
    pt[pt_i] = 0;

    if (table_empty(pt)) {
        uint64_t pt_phys = pde & ~0xFFFULL;
        pd[pd_i] = 0;
        to_free[free_count++] = pt_phys;

        if (table_empty(pd)) {
            uint64_t pd_phys = pdpte & ~0xFFFULL;
            pdpt[pdpt_i] = 0;
            to_free[free_count++] = pd_phys;

            if (table_empty(pdpt)) {
                uint64_t pdpt_phys = pml4e & ~0xFFFULL;
                pml4[pml4_i] = 0;
                to_free[free_count++] = pdpt_phys;
            }
        }
    }

    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)virt_addr) : "memory");
    vmm_unlock();

    for (size_t i = 0; i < free_count; i++) {
        pmm_free_page(to_free[i]);
    }
    return true;
}

bool vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags)
{
    return vmm_map_page_in(kernel_pml4, virt_addr, phys_addr, flags);
}

bool vmm_map_2m(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags)
{
    return vmm_map_2m_in(kernel_pml4, virt_addr, phys_addr, flags);
}

bool vmm_unmap_page(uint64_t virt_addr)
{
    return vmm_unmap_page_in(kernel_pml4, virt_addr);
}
