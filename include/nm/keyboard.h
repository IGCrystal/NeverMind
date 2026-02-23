#ifndef NM_KEYBOARD_H
#define NM_KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);
int keyboard_poll_char(void);

#endif
