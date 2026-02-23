#include "nm/mm.h"

#include <stdbool.h>
#include <stdint.h>

#define PAGE_SIZE 4096ULL
#define PAGE_FLAG_PRESENT 0x1ULL
#define PAGE_FLAG_RW 0x2ULL
#define PAGE_FLAG_PS 0x80ULL
#define KERNEL_VIRT_BASE 0xFFFF800000000000ULL

static uint64_t *kernel_pml4;

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
    return (uint64_t *)(uintptr_t)(phys + KERNEL_VIRT_BASE);
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

static uint64_t *get_or_create_next(uint64_t *table, uint16_t index, uint64_t flags)
{
    uint64_t entry = table[index];
    if ((entry & PAGE_FLAG_PRESENT) != 0) {
        return phys_to_ptr(entry & ~0xFFFULL);
    }

    uint64_t *next = alloc_table();
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

bool vmm_map_page(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags)
{
    uint16_t pml4_i = (virt_addr >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_i = (virt_addr >> 21) & 0x1FF;
    uint16_t pt_i = (virt_addr >> 12) & 0x1FF;

    uint64_t *pdpt = get_or_create_next(kernel_pml4, pml4_i, PAGE_FLAG_RW);
    if (pdpt == 0) {
        return false;
    }

    uint64_t *pd = get_or_create_next(pdpt, pdpt_i, PAGE_FLAG_RW);
    if (pd == 0) {
        return false;
    }

    uint64_t *pt = get_or_create_next(pd, pd_i, PAGE_FLAG_RW);
    if (pt == 0) {
        return false;
    }

    pt[pt_i] = (phys_addr & ~0xFFFULL) | (flags & 0xFFFULL) | PAGE_FLAG_PRESENT;
    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)virt_addr) : "memory");
    return true;
}

bool vmm_map_2m(uint64_t virt_addr, uint64_t phys_addr, uint64_t flags)
{
    uint16_t pml4_i = (virt_addr >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_i = (virt_addr >> 21) & 0x1FF;

    uint64_t *pdpt = get_or_create_next(kernel_pml4, pml4_i, PAGE_FLAG_RW);
    if (pdpt == 0) {
        return false;
    }

    uint64_t *pd = get_or_create_next(pdpt, pdpt_i, PAGE_FLAG_RW);
    if (pd == 0) {
        return false;
    }

    pd[pd_i] = (phys_addr & ~0x1FFFFFULL) | (flags & 0xFFFULL) | PAGE_FLAG_PRESENT | PAGE_FLAG_PS;
    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)virt_addr) : "memory");
    return true;
}

bool vmm_unmap_page(uint64_t virt_addr)
{
    uint16_t pml4_i = (virt_addr >> 39) & 0x1FF;
    uint16_t pdpt_i = (virt_addr >> 30) & 0x1FF;
    uint16_t pd_i = (virt_addr >> 21) & 0x1FF;
    uint16_t pt_i = (virt_addr >> 12) & 0x1FF;

    if ((kernel_pml4[pml4_i] & PAGE_FLAG_PRESENT) == 0) {
        return false;
    }

    uint64_t *pdpt = phys_to_ptr(kernel_pml4[pml4_i] & ~0xFFFULL);
    if ((pdpt[pdpt_i] & PAGE_FLAG_PRESENT) == 0) {
        return false;
    }

    uint64_t *pd = phys_to_ptr(pdpt[pdpt_i] & ~0xFFFULL);
    if ((pd[pd_i] & PAGE_FLAG_PRESENT) == 0 || (pd[pd_i] & PAGE_FLAG_PS) != 0) {
        return false;
    }

    uint64_t *pt = phys_to_ptr(pd[pd_i] & ~0xFFFULL);
    pt[pt_i] = 0;
    __asm__ volatile("invlpg (%0)" : : "r"((void *)(uintptr_t)virt_addr) : "memory");
    return true;
}
