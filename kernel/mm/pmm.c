#include "nm/mm.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef NEVERMIND_HOST_TEST
#include <stdlib.h>
#endif

#include "nm/multiboot2.h"
#include "nm/string.h"

#define PAGE_SIZE 4096ULL
#define MAX_PHYS_MEM_BYTES (128ULL * 1024ULL * 1024ULL * 1024ULL)
#define MAX_FRAMES (MAX_PHYS_MEM_BYTES / PAGE_SIZE)
#define BITMAP_WORD_BITS 64ULL
#define BITMAP_WORDS (MAX_FRAMES / BITMAP_WORD_BITS)

static struct nm_mm_stats mm_stats;

#ifndef NEVERMIND_HOST_TEST
static uint64_t frame_bitmap[BITMAP_WORDS];
#endif

#ifdef NEVERMIND_HOST_TEST

struct host_alloc_entry {
    uint64_t key;
    void *ptr;
    bool used;
};

#define HOST_MAX_ALLOCS 32768

static struct host_alloc_entry host_allocs[HOST_MAX_ALLOCS];
static uint64_t host_alloc_key_seed;

static uint64_t host_make_key(void)
{
    host_alloc_key_seed += PAGE_SIZE;
    return host_alloc_key_seed | 1ULL;
}

void pmm_init_from_ranges(const struct nm_mem_range *ranges, size_t count)
{
    (void)ranges;

    mm_stats.total_frames = 0;
    mm_stats.free_frames = 0;
    mm_stats.used_frames = 0;
    mm_stats.reserved_frames = 0;

    for (size_t i = 0; i < count; i++) {
        if (ranges[i].type == NM_MEM_AVAILABLE) {
            mm_stats.total_frames += ranges[i].length / PAGE_SIZE;
        }
    }

    mm_stats.reserved_frames = 256;
    if (mm_stats.total_frames > mm_stats.reserved_frames) {
        mm_stats.free_frames = mm_stats.total_frames - mm_stats.reserved_frames;
    } else {
        mm_stats.free_frames = 0;
    }
    mm_stats.used_frames = mm_stats.total_frames - mm_stats.free_frames;

    for (size_t i = 0; i < HOST_MAX_ALLOCS; i++) {
        if (host_allocs[i].used) {
            free(host_allocs[i].ptr);
        }
        host_allocs[i].used = false;
        host_allocs[i].ptr = 0;
        host_allocs[i].key = 0;
    }
    host_alloc_key_seed = 0x1000;
}

void pmm_init_from_multiboot2(uint64_t mb2_info_ptr)
{
    (void)mb2_info_ptr;
    const struct nm_mem_range default_ranges[] = {
        {.base = 0x00100000, .length = 64ULL * 1024ULL * 1024ULL, .type = NM_MEM_AVAILABLE},
    };
    pmm_init_from_ranges(default_ranges, 1);
}

uint64_t pmm_alloc_page(void)
{
    if (mm_stats.free_frames == 0) {
        return 0;
    }

    for (size_t i = 0; i < HOST_MAX_ALLOCS; i++) {
        if (!host_allocs[i].used) {
            void *ptr = calloc(1, PAGE_SIZE);
            if (ptr == 0) {
                return 0;
            }

            host_allocs[i].used = true;
            host_allocs[i].ptr = ptr;
            host_allocs[i].key = host_make_key();
            mm_stats.free_frames--;
            mm_stats.used_frames++;
            return host_allocs[i].key;
        }
    }

    return 0;
}

void pmm_free_page(uint64_t phys_addr)
{
    if (phys_addr == 0) {
        return;
    }

    for (size_t i = 0; i < HOST_MAX_ALLOCS; i++) {
        if (host_allocs[i].used && host_allocs[i].key == phys_addr) {
            free(host_allocs[i].ptr);
            host_allocs[i].used = false;
            host_allocs[i].ptr = 0;
            host_allocs[i].key = 0;
            mm_stats.free_frames++;
            mm_stats.used_frames--;
            return;
        }
    }
}

struct nm_mm_stats pmm_get_stats(void)
{
    return mm_stats;
}

void *pmm_host_ptr_from_key(uint64_t key)
{
    for (size_t i = 0; i < HOST_MAX_ALLOCS; i++) {
        if (host_allocs[i].used && host_allocs[i].key == key) {
            return host_allocs[i].ptr;
        }
    }
    return 0;
}

#else

extern uint8_t __kernel_phys_start[];
extern uint8_t __kernel_phys_end[];

static inline bool frame_valid(uint64_t frame)
{
    return frame < MAX_FRAMES;
}

static inline void set_frame(uint64_t frame)
{
    if (!frame_valid(frame)) {
        return;
    }
    frame_bitmap[frame / BITMAP_WORD_BITS] |= (1ULL << (frame % BITMAP_WORD_BITS));
}

static inline void clear_frame(uint64_t frame)
{
    if (!frame_valid(frame)) {
        return;
    }
    frame_bitmap[frame / BITMAP_WORD_BITS] &= ~(1ULL << (frame % BITMAP_WORD_BITS));
}

static inline bool test_frame(uint64_t frame)
{
    if (!frame_valid(frame)) {
        return true;
    }
    return (frame_bitmap[frame / BITMAP_WORD_BITS] & (1ULL << (frame % BITMAP_WORD_BITS))) != 0;
}

static void mark_all_used(void)
{
    memset(frame_bitmap, 0xFF, sizeof(frame_bitmap));
    mm_stats.total_frames = MAX_FRAMES;
    mm_stats.free_frames = 0;
    mm_stats.used_frames = MAX_FRAMES;
    mm_stats.reserved_frames = 0;
}

static void mark_range_available(uint64_t base, uint64_t length)
{
    uint64_t start = (base + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    uint64_t end = (base + length) / PAGE_SIZE;
    if (end > MAX_FRAMES) {
        end = MAX_FRAMES;
    }

    for (uint64_t frame = start; frame < end; frame++) {
        if (test_frame(frame)) {
            clear_frame(frame);
            mm_stats.free_frames++;
            mm_stats.used_frames--;
        }
    }
}

static void reserve_range(uint64_t base, uint64_t length)
{
    uint64_t start = base / PAGE_SIZE;
    uint64_t end = (base + length + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    if (end > MAX_FRAMES) {
        end = MAX_FRAMES;
    }

    for (uint64_t frame = start; frame < end; frame++) {
        if (!test_frame(frame)) {
            set_frame(frame);
            mm_stats.free_frames--;
            mm_stats.used_frames++;
            mm_stats.reserved_frames++;
        }
    }
}

void pmm_init_from_ranges(const struct nm_mem_range *ranges, size_t count)
{
    mark_all_used();

    for (size_t i = 0; i < count; i++) {
        if (ranges[i].type == NM_MEM_AVAILABLE) {
            mark_range_available(ranges[i].base, ranges[i].length);
        }
    }

    reserve_range(0, 0x100000);
}

void pmm_init_from_multiboot2(uint64_t mb2_info_ptr)
{
    if (mb2_info_ptr == 0) {
        mark_all_used();
        reserve_range(0, 0x100000);
        reserve_range((uint64_t)(uintptr_t)__kernel_phys_start,
                      (uint64_t)(uintptr_t)(__kernel_phys_end - __kernel_phys_start));
        return;
    }

    const struct mb2_info_header *hdr = (const struct mb2_info_header *)(uintptr_t)mb2_info_ptr;
    const uint8_t *cursor = (const uint8_t *)(uintptr_t)(mb2_info_ptr + 8);
    const uint8_t *end = (const uint8_t *)(uintptr_t)(mb2_info_ptr + hdr->total_size);

    mark_all_used();

    while (cursor < end) {
        const struct mb2_tag *tag = (const struct mb2_tag *)cursor;
        if (tag->type == MB2_TAG_END) {
            break;
        }

        if (tag->type == MB2_TAG_MMAP) {
            const struct mb2_tag_mmap *mmap_tag = (const struct mb2_tag_mmap *)tag;
            const uint8_t *entry_ptr = cursor + sizeof(struct mb2_tag_mmap);
            const uint8_t *entry_end = cursor + tag->size;

            while (entry_ptr < entry_end) {
                const struct mb2_mmap_entry *e = (const struct mb2_mmap_entry *)entry_ptr;
                if (e->type == NM_MEM_AVAILABLE) {
                    mark_range_available(e->addr, e->len);
                }
                entry_ptr += mmap_tag->entry_size;
            }
        }

        cursor += (tag->size + 7U) & ~7U;
    }

    reserve_range(0, 0x100000);

    // Do not allocate pages that overlap the kernel image, boot stack, or boot page tables.
    reserve_range((uint64_t)(uintptr_t)__kernel_phys_start,
                  (uint64_t)(uintptr_t)(__kernel_phys_end - __kernel_phys_start));

    // Keep the multiboot2 info structure intact after parsing.
    reserve_range(mb2_info_ptr, hdr->total_size);
}

uint64_t pmm_alloc_page(void)
{
    for (uint64_t frame = 0; frame < MAX_FRAMES; frame++) {
        if (!test_frame(frame)) {
            set_frame(frame);
            mm_stats.free_frames--;
            mm_stats.used_frames++;
            return frame * PAGE_SIZE;
        }
    }

    return 0;
}

void pmm_free_page(uint64_t phys_addr)
{
    if (phys_addr == 0 || (phys_addr % PAGE_SIZE) != 0) {
        return;
    }

    uint64_t frame = phys_addr / PAGE_SIZE;
    if (!frame_valid(frame) || !test_frame(frame)) {
        return;
    }

    clear_frame(frame);
    mm_stats.free_frames++;
    mm_stats.used_frames--;
}

struct nm_mm_stats pmm_get_stats(void)
{
    return mm_stats;
}

#endif
