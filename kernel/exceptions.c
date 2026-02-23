#include <stdint.h>

#include "nm/console.h"

static void console_write_hex_u64(uint64_t value)
{
    static const char hex[] = "0123456789abcdef";
    console_write("0x");
    for (int i = 15; i >= 0; i--) {
        uint8_t nibble = (uint8_t)((value >> (i * 4)) & 0xFULL);
        console_putc(hex[nibble]);
    }
}

__attribute__((noreturn)) void nm_exception_dispatch(const uint64_t *stack)
{
    uint64_t vec = stack[0];
    uint64_t err = stack[1];
    uint64_t rip = stack[2];
    uint64_t cs = stack[3];
    uint64_t rflags = stack[4];
    uint64_t cr2 = 0;
    if (vec == 14) {
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    }

    console_write("[EXC] vec=");
    console_write_hex_u64(vec);
    console_write(" err=");
    console_write_hex_u64(err);
    console_write(" rip=");
    console_write_hex_u64(rip);
    console_write(" cs=");
    console_write_hex_u64(cs);
    console_write(" rflags=");
    console_write_hex_u64(rflags);
    if (vec == 14) {
        console_write(" cr2=");
        console_write_hex_u64(cr2);
    }
    console_write(" rsp=");
    console_write_hex_u64((uint64_t)(uintptr_t)stack);
    console_write("\n");

    __asm__ volatile("cli");
    for (;;) {
        __asm__ volatile("hlt");
    }
}
