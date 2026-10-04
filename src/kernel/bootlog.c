#include "bootlog.h"
#include "kprintf.h"
#include "version.h"
#include "vga.h"

void bootlog_banner(void)
{
    vga_set_color(VGA_LIGHT_MAGENTA, VGA_BLACK);
    kprintf("\n  %s OS %s\n\n", ASTRO_NAME, ASTRO_VERSION);
}

void bootlog_step(const char *what)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("  [ ");
    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("OK");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf(" ] %s\n", what);
}
