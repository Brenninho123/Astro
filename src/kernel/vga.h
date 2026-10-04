#ifndef ASTRO_VGA_H
#define ASTRO_VGA_H

#include "types.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

enum vga_color {
    VGA_BLACK = 0,
    VGA_BLUE,
    VGA_GREEN,
    VGA_CYAN,
    VGA_RED,
    VGA_MAGENTA,
    VGA_BROWN,
    VGA_LIGHT_GREY,
    VGA_DARK_GREY,
    VGA_LIGHT_BLUE,
    VGA_LIGHT_GREEN,
    VGA_LIGHT_CYAN,
    VGA_LIGHT_RED,
    VGA_LIGHT_MAGENTA,
    VGA_YELLOW,
    VGA_WHITE,
};

/* Useful CP437 characters for drawing interface elements */
#define CH_UPPER_HALF 0xDF
#define CH_LOWER_HALF 0xDC
#define CH_FULL_BLOCK 0xDB

static inline uint8_t vga_attr(enum vga_color fg, enum vga_color bg)
{
    return (uint8_t)(fg | (bg << 4));
}

void vga_init(void);
void vga_clear(void);
void vga_set_color(enum vga_color fg, enum vga_color bg);

/* Stream output (uses the cursor) */
void vga_putc(char c);
void vga_puts(const char *s);

/* Positional output (does not move the cursor) */
void vga_putc_at(int x, int y, uint8_t ch, uint8_t attr);
void vga_puts_at(int x, int y, const char *s, uint8_t attr);
void vga_fill(int x, int y, int w, int h, uint8_t ch, uint8_t attr);

void vga_set_cursor(int x, int y);
void vga_cursor_enable(bool enable);

/*
 * Platform hooks. The text-mode logic in vga.c is shared; each platform
 * provides the backing buffer and the cursor control:
 *   - x86 (src/arch/x86/vga_hw.c): the real VGA memory at 0xB8000.
 *   - Windows simulator (src/sim/console.c): an in-memory buffer drawn to the console.
 * The buffer holds VGA_WIDTH * VGA_HEIGHT cells: low byte = CP437 character,
 * high byte = attribute (foreground | background << 4).
 */
volatile uint16_t *vga_hw_buffer(void);
void vga_hw_set_cursor(int x, int y);
void vga_hw_cursor_enable(bool enable);

#endif
