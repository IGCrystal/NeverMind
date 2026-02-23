#include <stdint.h>

#include "nm/console.h"
#include "nm/io.h"
#include "nm/klog.h"

#define COM1 0x3F8
#define VGA_MEM ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25

static uint16_t vga_row;
static uint16_t vga_col;
static uint8_t vga_attr = 0x07;

static void serial_init(void)
{
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c)
{
    while ((inb(COM1 + 5) & 0x20) == 0) {
    }
    outb(COM1, (uint8_t)c);
}

static void vga_scroll(void)
{
    if (vga_row < VGA_ROWS) {
        return;
    }

    for (uint16_t r = 1; r < VGA_ROWS; r++) {
        for (uint16_t c = 0; c < VGA_COLS; c++) {
            VGA_MEM[(r - 1) * VGA_COLS + c] = VGA_MEM[r * VGA_COLS + c];
        }
    }

    for (uint16_t c = 0; c < VGA_COLS; c++) {
        VGA_MEM[(VGA_ROWS - 1) * VGA_COLS + c] = ((uint16_t)vga_attr << 8) | ' ';
    }
    vga_row = VGA_ROWS - 1;
}

static void vga_putc(char c)
{
    if (c == '\n') {
        vga_row++;
        vga_col = 0;
        vga_scroll();
        return;
    }

    VGA_MEM[vga_row * VGA_COLS + vga_col] = ((uint16_t)vga_attr << 8) | (uint8_t)c;
    vga_col++;
    if (vga_col >= VGA_COLS) {
        vga_col = 0;
        vga_row++;
        vga_scroll();
    }
}

void console_init(void)
{
    vga_row = 0;
    vga_col = 0;
    serial_init();
}

void console_putc(char c)
{
    klog_putc(c);
    serial_putc(c);
    vga_putc(c);
}

void console_write(const char *str)
{
    while (*str != '\0') {
        console_putc(*str++);
    }
}
