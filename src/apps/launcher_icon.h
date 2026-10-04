#ifndef ASTRO_LAUNCHER_ICON_H
#define ASTRO_LAUNCHER_ICON_H

#include "types.h"

/* Astro Launcher icon: a ringed planet with stars, 16x16 pixels.
 * Vector source: assets/astro-launcher.svg */
#define ICON_PIXELS 16
#define ICON_COLS   ICON_PIXELS
#define ICON_ROWS   (ICON_PIXELS / 2)    /* 2 pixels per cell (half block) */

/* Draws the icon in text mode with cell (x, y) as the top-left corner. */
void launcher_icon_draw(int x, int y);

#endif
