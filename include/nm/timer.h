#ifndef NM_TIMER_H
#define NM_TIMER_H

#include <stdint.h>

void pit_init(uint32_t hz);
uint64_t pit_ticks(void);

#endif
