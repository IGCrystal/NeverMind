#ifndef NM_MULTIBOOT2_H
#define NM_MULTIBOOT2_H

#include <stdint.h>

struct mb2_info_header {
    uint32_t total_size;
    uint32_t reserved;
};

struct mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct mb2_tag_mmap {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

struct mb2_mmap_entry {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t zero;
};

enum {
    MB2_TAG_END = 0,
    MB2_TAG_MMAP = 6,
};

#endif
