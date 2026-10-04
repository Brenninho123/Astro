#include "vga.h"

static volatile uint16_t *mem;
static int     cursor_x;
static int     cursor_y;
static uint8_t current_attr = 0x07;     /* light grey on black */

static inline uint16_t cell(uint8_t ch, uint8_t attr)
{
    return (uint16_t)ch | ((uint16_t)attr << 8);
}

static void scroll(void)
{
    for (int y = 1; y < VGA_HEIGHT; y++)
        for (int x = 0; x < VGA_WIDTH; x++)
            mem[(y - 1) * VGA_WIDTH + x] = mem[y * VGA_WIDTH + x];

    for (int x = 0; x < VGA_WIDTH; x++)
        mem[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = cell(' ', current_attr);

    cursor_y = VGA_HEIGHT - 1;
}

void vga_init(void)
{
    mem = vga_hw_buffer();
    current_attr = vga_attr(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_cursor_enable(true);
}

void vga_clear(void)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        mem[i] = cell(' ', current_attr);
    cursor_x = 0;
    cursor_y = 0;
    vga_hw_set_cursor(cursor_x, cursor_y);
}

void vga_set_color(enum vga_color fg, enum vga_color bg)
{
    current_attr = vga_attr(fg, bg);
}

void vga_putc(char c)
{
    switch (c) {
    case '\n':
        cursor_x = 0;
        cursor_y++;
        break;
    case '\r':
        cursor_x = 0;
        break;
    case '\t':
        cursor_x = (cursor_x + 4) & ~3;
        break;
    case '\b':
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = VGA_WIDTH - 1;
        }
        mem[cursor_y * VGA_WIDTH + cursor_x] = cell(' ', current_attr);
        break;
    default:
        mem[cursor_y * VGA_WIDTH + cursor_x] = cell((uint8_t)c, current_attr);
        cursor_x++;
        break;
    }

    if (cursor_x >= VGA_WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VGA_HEIGHT)
        scroll();

    vga_hw_set_cursor(cursor_x, cursor_y);
}

void vga_puts(const char *s)
{
    while (*s)
        vga_putc(*s++);
}

void vga_putc_at(int x, int y, uint8_t ch, uint8_t attr)
{
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT)
        return;
    mem[y * VGA_WIDTH + x] = cell(ch, attr);
}

void vga_puts_at(int x, int y, const char *s, uint8_t attr)
{
    while (*s)
        vga_putc_at(x++, y, (uint8_t)*s++, attr);
}

void vga_fill(int x, int y, int w, int h, uint8_t ch, uint8_t attr)
{
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            vga_putc_at(x + i, y + j, ch, attr);
}

void vga_set_cursor(int x, int y)
{
    cursor_x = x;
    cursor_y = y;
    vga_hw_set_cursor(cursor_x, cursor_y);
}

void vga_cursor_enable(bool enable)
{
    vga_hw_cursor_enable(enable);
}
