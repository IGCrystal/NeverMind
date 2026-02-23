#include "nm/string.h"

void *memset(void *dest, int value, size_t count)
{
    unsigned char *ptr = (unsigned char *)dest;
    for (size_t i = 0; i < count; i++) {
        ptr[i] = (unsigned char)value;
    }
    return dest;
}
