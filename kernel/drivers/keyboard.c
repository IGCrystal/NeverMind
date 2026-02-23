#include "nm/keyboard.h"

#include <stdint.h>

#include "nm/io.h"
#include "nm/irq.h"

#define KBD_DATA_PORT 0x60
#define KBD_STATUS_PORT 0x64

static int g_last_char = -1;

static char scancode_to_ascii(uint8_t sc)
{
    static const char map[128] = {
        0,   27,  '1', '2', '3', '4', '5', '6',
        '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
        'o', 'p', '[', ']', '\n', 0,   'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
        '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', 0,   '*',
        0,   ' ',
    };
    if (sc >= 128) {
        return 0;
    }
    return map[sc];
}

static void keyboard_irq_top(int irq, void *ctx)
{
    (void)irq;
    (void)ctx;

#ifdef NEVERMIND_HOST_TEST
    return;
#else
    if ((inb(KBD_STATUS_PORT) & 0x01) == 0) {
        return;
    }
    uint8_t sc = inb(KBD_DATA_PORT);
    if (sc & 0x80) {
        return;
    }
    char c = scancode_to_ascii(sc);
    if (c != 0) {
        g_last_char = (int)c;
    }
#endif
}

void keyboard_init(void)
{
    (void)irq_register(33, keyboard_irq_top, 0, 0, "keyboard");
}

int keyboard_poll_char(void)
{
    int c = g_last_char;
    g_last_char = -1;
    return c;
}
