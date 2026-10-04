#include "launcher_icon.h"
#include "vga.h"

/*
 * Legend:  .  empty (black)         M  light magenta (planet light side)
 *          m  magenta               1  blue (planet shadow)
 *          y  yellow (ring)         w  white (star)
 *          c  light cyan (star)
 *
 * tools/make-icon.ps1 reads this grid to generate assets/astro.ico,
 * so keep the rows as 16-character string literals.
 */
static const char *const icon[ICON_PIXELS] = {
    "................",
    ".w..............",
    ".............w..",
    ".....MMMmmm.....",
    "....MMMmmmmm....",
    "...MMMmmmmmmmyy.",
    "...MMmmmmmmyy...",
    "...Mmmmmmyymm...",
    "...mmmmyymmm1...",
    "...mmyymmmm11.c.",
    "...yymmmmm111...",
    ".yy.mmmm1111....",
    ".....mm1111.....",
    "..c.............",
    "............w...",
    "................",
};

static enum vga_color pixel_color(char c)
{
    switch (c) {
    case 'M': return VGA_LIGHT_MAGENTA;
    case 'm': return VGA_MAGENTA;
    case '1': return VGA_BLUE;
    case 'y': return VGA_YELLOW;
    case 'w': return VGA_WHITE;
    case 'c': return VGA_LIGHT_CYAN;
    default:  return VGA_BLACK;
    }
}

void launcher_icon_draw(int x, int y)
{
    /* Each cell uses the upper half block: foreground = top pixel,
     * background = bottom pixel. This doubles the vertical resolution. */
    for (int row = 0; row < ICON_ROWS; row++) {
        for (int col = 0; col < ICON_COLS; col++) {
            enum vga_color top = pixel_color(icon[row * 2][col]);
            enum vga_color bottom = pixel_color(icon[row * 2 + 1][col]);
            vga_putc_at(x + col, y + row, CH_UPPER_HALF, vga_attr(top, bottom));
        }
    }
}
