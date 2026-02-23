#include "nm/mm.h"

#include <stddef.h>
#include <stdint.h>

#include "nm/string.h"

#define PAGE_SIZE 4096ULL
#define KMALLOC_MAGIC 0x4D4E564BUL

struct kmalloc_header {
    uint32_t magic;
    uint32_t size;
    struct kmalloc_header *next;
};

static struct kmalloc_header *free_list;

void mm_init(uint64_t mb2_info_ptr)
{
    pmm_init_from_multiboot2(mb2_info_ptr);
#ifndef NEVERMIND_HOST_TEST
    vmm_init();
#endif
}

void *kmalloc(size_t size)
{
    if (size == 0) {
        return 0;
    }

    struct kmalloc_header **prev = &free_list;
    struct kmalloc_header *cur = free_list;
    while (cur != 0) {
        if (cur->size >= size) {
            *prev = cur->next;
            cur->next = 0;
            return (void *)(cur + 1);
        }
        prev = &cur->next;
        cur = cur->next;
    }

    size_t total = sizeof(struct kmalloc_header) + size;
    size_t pages = (total + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    uint64_t first_page = pmm_alloc_pages(pages);
    if (first_page == 0) {
        return 0;
    }

    struct kmalloc_header *header;
#ifdef NEVERMIND_HOST_TEST
    header = (struct kmalloc_header *)pmm_host_ptr_from_key(first_page);
#else
    header = (struct kmalloc_header *)(uintptr_t)first_page;
#endif
    header->magic = KMALLOC_MAGIC;
    header->size = (uint32_t)(pages * PAGE_SIZE - sizeof(*header));
    header->next = 0;

    if (header->size > size) {
        size_t remain = header->size - size;
        if (remain > sizeof(struct kmalloc_header) + 32) {
            struct kmalloc_header *split = (struct kmalloc_header *)((uint8_t *)(header + 1) + size);
            split->magic = KMALLOC_MAGIC;
            split->size = (uint32_t)(remain - sizeof(struct kmalloc_header));
            split->next = free_list;
            free_list = split;
            header->size = (uint32_t)size;
        }
    }

    return (void *)(header + 1);
}

void kfree(void *ptr)
{
    if (ptr == 0) {
        return;
    }

    struct kmalloc_header *header = ((struct kmalloc_header *)ptr) - 1;
    if (header->magic != KMALLOC_MAGIC) {
        return;
    }

    header->next = free_list;
    free_list = header;
}
