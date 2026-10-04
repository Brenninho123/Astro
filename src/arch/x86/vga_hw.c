/* VGA text-mode hardware backend (see the platform hooks in src/kernel/vga.h). */
#include "vga.h"
#include "io.h"

volatile uint16_t *vga_hw_buffer(void)
{
    return (volatile uint16_t *)0xB8000;
}

void vga_hw_set_cursor(int x, int y)
{
    uint16_t pos = (uint16_t)(y * VGA_WIDTH + x);
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)(pos >> 8));
}

void vga_hw_cursor_enable(bool enable)
{
    if (enable) {
        outb(0x3D4, 0x0A);
        outb(0x3D5, (inb(0x3D5) & 0xC0) | 13);
        outb(0x3D4, 0x0B);
        outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
    } else {
        outb(0x3D4, 0x0A);
        outb(0x3D5, 0x20);
    }
}
