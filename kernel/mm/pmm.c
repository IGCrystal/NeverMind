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
#define KERNEL_VIRT_BASE 0xFFFFFFFF80000000ULL
#define MAX_PHYS_MEM_BYTES (128ULL * 1024ULL * 1024ULL * 1024ULL)
#define MAX_FRAMES (MAX_PHYS_MEM_BYTES / PAGE_SIZE)
#define BITMAP_WORD_BITS 64ULL

static struct nm_mm_stats mm_stats;

#ifndef NEVERMIND_HOST_TEST
static uint64_t *frame_bitmap;
static uint64_t frame_limit;
static uint64_t bitmap_words;
static uint64_t bitmap_phys_base;
static uint64_t bitmap_bytes;
static uint64_t alloc_word_cursor;
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

static inline uint64_t align_up_u64(uint64_t value, uint64_t align)
{
    return (value + align - 1ULL) & ~(align - 1ULL);
}

static inline uint64_t kernel_symbol_to_phys(const void *sym)
{
    uint64_t addr = (uint64_t)(uintptr_t)sym;
    if (addr >= KERNEL_VIRT_BASE) {
        return addr - KERNEL_VIRT_BASE;
    }
    return addr;
}

static void init_runtime_bitmap(uint64_t frames, uint64_t min_phys_base)
{
    if (frames == 0) {
        frames = 1;
    }
    if (frames > MAX_FRAMES) {
        frames = MAX_FRAMES;
    }

    frame_limit = frames;
    bitmap_words = (frame_limit + BITMAP_WORD_BITS - 1ULL) / BITMAP_WORD_BITS;
    bitmap_bytes = bitmap_words * sizeof(uint64_t);

    uint64_t kernel_end_phys = kernel_symbol_to_phys(__kernel_phys_end);
    if (min_phys_base < kernel_end_phys) {
        min_phys_base = kernel_end_phys;
    }

    bitmap_phys_base = align_up_u64(min_phys_base, PAGE_SIZE);
    frame_bitmap = (uint64_t *)(uintptr_t)bitmap_phys_base;
}

static inline bool frame_valid(uint64_t frame)
{
    return frame < frame_limit;
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
    memset(frame_bitmap, 0xFF, bitmap_bytes);
    mm_stats.total_frames = frame_limit;
    mm_stats.free_frames = 0;
    mm_stats.used_frames = frame_limit;
    mm_stats.reserved_frames = 0;
    alloc_word_cursor = 0;
}

static void mark_range_available(uint64_t base, uint64_t length)
{
    uint64_t start = (base + PAGE_SIZE - 1ULL) / PAGE_SIZE;
    uint64_t end = (base + length) / PAGE_SIZE;
    if (end > MAX_FRAMES) {
        end = frame_limit;
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
        end = frame_limit;
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
    uint64_t highest_end = 0;
    for (size_t i = 0; i < count; i++) {
        if (ranges[i].type != NM_MEM_AVAILABLE) {
            continue;
        }
        uint64_t end = ranges[i].base + ranges[i].length;
        if (end > highest_end) {
            highest_end = end;
        }
    }

    uint64_t detected_frames = highest_end / PAGE_SIZE;
    init_runtime_bitmap(detected_frames, kernel_symbol_to_phys(__kernel_phys_end));

    mark_all_used();

    for (size_t i = 0; i < count; i++) {
        if (ranges[i].type == NM_MEM_AVAILABLE) {
            mark_range_available(ranges[i].base, ranges[i].length);
        }
    }

    reserve_range(0, 0x100000);
    reserve_range(kernel_symbol_to_phys(__kernel_phys_start),
                  kernel_symbol_to_phys(__kernel_phys_end) - kernel_symbol_to_phys(__kernel_phys_start));
    reserve_range(bitmap_phys_base, bitmap_bytes);
}

void pmm_init_from_multiboot2(uint64_t mb2_info_ptr)
{
    if (mb2_info_ptr == 0) {
        init_runtime_bitmap(MAX_FRAMES, kernel_symbol_to_phys(__kernel_phys_end));
        mark_all_used();
        reserve_range(0, 0x100000);
        reserve_range(kernel_symbol_to_phys(__kernel_phys_start),
                      kernel_symbol_to_phys(__kernel_phys_end) - kernel_symbol_to_phys(__kernel_phys_start));
        reserve_range(bitmap_phys_base, bitmap_bytes);
        return;
    }

    const struct mb2_info_header *hdr = (const struct mb2_info_header *)(uintptr_t)mb2_info_ptr;
    if (hdr->total_size < sizeof(struct mb2_info_header)) {
        init_runtime_bitmap(MAX_FRAMES, kernel_symbol_to_phys(__kernel_phys_end));
        mark_all_used();
        reserve_range(0, 0x100000);
        reserve_range(kernel_symbol_to_phys(__kernel_phys_start),
                      kernel_symbol_to_phys(__kernel_phys_end) - kernel_symbol_to_phys(__kernel_phys_start));
        reserve_range(bitmap_phys_base, bitmap_bytes);
        return;
    }

    const uint8_t *cursor;
    const uint8_t *end = (const uint8_t *)(uintptr_t)(mb2_info_ptr + hdr->total_size);
    uint64_t highest_end = 0;

    cursor = (const uint8_t *)(uintptr_t)(mb2_info_ptr + 8);
    while (cursor + sizeof(struct mb2_tag) <= end) {
        const struct mb2_tag *tag = (const struct mb2_tag *)cursor;
        if (tag->type == MB2_TAG_END) {
            break;
        }

        if (tag->size < sizeof(struct mb2_tag)) {
            break;
        }

        uint64_t aligned = ((uint64_t)tag->size + 7ULL) & ~7ULL;
        const uint8_t *next = cursor + aligned;
        if (next <= cursor || next > end) {
            break;
        }

        if (tag->type == MB2_TAG_MMAP) {
            if (tag->size >= sizeof(struct mb2_tag_mmap)) {
            const struct mb2_tag_mmap *mmap_tag = (const struct mb2_tag_mmap *)tag;
            const uint8_t *entry_ptr = cursor + sizeof(struct mb2_tag_mmap);
            const uint8_t *entry_end = cursor + tag->size;
                if (mmap_tag->entry_size >= sizeof(struct mb2_mmap_entry)) {
            while (entry_ptr + sizeof(struct mb2_mmap_entry) <= entry_end) {
                const struct mb2_mmap_entry *e = (const struct mb2_mmap_entry *)entry_ptr;
                if (e->type == NM_MEM_AVAILABLE) {
                    uint64_t e_end = e->addr + e->len;
                    if (e_end > highest_end) {
                        highest_end = e_end;
                    }
                }
                entry_ptr += mmap_tag->entry_size;
            }
                }
            }
        }

        cursor = next;
    }

    init_runtime_bitmap(highest_end / PAGE_SIZE, mb2_info_ptr + hdr->total_size);

    mark_all_used();

    cursor = (const uint8_t *)(uintptr_t)(mb2_info_ptr + 8);
    while (cursor + sizeof(struct mb2_tag) <= end) {
        const struct mb2_tag *tag = (const struct mb2_tag *)cursor;
        if (tag->type == MB2_TAG_END) {
            break;
        }

        if (tag->size < sizeof(struct mb2_tag)) {
            break;
        }

        uint64_t aligned = ((uint64_t)tag->size + 7ULL) & ~7ULL;
        const uint8_t *next = cursor + aligned;
        if (next <= cursor || next > end) {
            break;
        }

        if (tag->type == MB2_TAG_MMAP) {
            if (tag->size >= sizeof(struct mb2_tag_mmap)) {
                const struct mb2_tag_mmap *mmap_tag = (const struct mb2_tag_mmap *)tag;
                const uint8_t *entry_ptr = cursor + sizeof(struct mb2_tag_mmap);
                const uint8_t *entry_end = cursor + tag->size;

                if (mmap_tag->entry_size >= sizeof(struct mb2_mmap_entry)) {
                    while (entry_ptr + sizeof(struct mb2_mmap_entry) <= entry_end) {
                        const struct mb2_mmap_entry *e = (const struct mb2_mmap_entry *)entry_ptr;
                        if (e->type == NM_MEM_AVAILABLE) {
                            mark_range_available(e->addr, e->len);
                        }
                        entry_ptr += mmap_tag->entry_size;
                    }
                }
            }
        }

        cursor = next;
    }

    reserve_range(0, 0x100000);

    // Do not allocate pages that overlap the kernel image, boot stack, or boot page tables.
    reserve_range(kernel_symbol_to_phys(__kernel_phys_start),
                  kernel_symbol_to_phys(__kernel_phys_end) - kernel_symbol_to_phys(__kernel_phys_start));

    // Keep PMM bitmap pages reserved.
    reserve_range(bitmap_phys_base, bitmap_bytes);

    // Keep the multiboot2 info structure intact after parsing.
    reserve_range(mb2_info_ptr, hdr->total_size);
}

uint64_t pmm_alloc_page(void)
{
    if (mm_stats.free_frames == 0) {
        return 0;
    }

    uint64_t start = alloc_word_cursor;
    for (uint64_t pass = 0; pass < bitmap_words; pass++) {
        uint64_t word_idx = (start + pass) % bitmap_words;
        uint64_t word = frame_bitmap[word_idx];
        if (word == UINT64_MAX) {
            continue;
        }

        uint64_t free_mask = ~word;
        if (free_mask == 0) {
            continue;
        }

        uint64_t bit = (uint64_t)__builtin_ctzll(free_mask);
        uint64_t frame = word_idx * BITMAP_WORD_BITS + bit;
        if (!frame_valid(frame)) {
            continue;
        }

        set_frame(frame);
        mm_stats.free_frames--;
        mm_stats.used_frames++;
        alloc_word_cursor = word_idx;
        return frame * PAGE_SIZE;
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
    alloc_word_cursor = frame / BITMAP_WORD_BITS;
}

struct nm_mm_stats pmm_get_stats(void)
{
    return mm_stats;
}

#endif
