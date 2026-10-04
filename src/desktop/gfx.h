#ifndef ASTRO_GFX_H
#define ASTRO_GFX_H

#include <stdint.h>

/* Tiny software renderer used by the graphical home screen.
 * Pixels are 0x00RRGGBB, top-down, no padding. Alpha arguments are 0..255. */

typedef struct {
    uint32_t *px;
    int w, h;
} gfx_t;

uint32_t gfx_mix(uint32_t a, uint32_t b, int t);            /* t: 0 = a, 255 = b */

void gfx_clear(gfx_t *g, uint32_t c);
void gfx_put(gfx_t *g, int x, int y, uint32_t c, int a);
void gfx_rect(gfx_t *g, int x, int y, int w, int h, uint32_t c, int a);

/* Anti-aliased rounded rectangles and circles. */
void gfx_rrect(gfx_t *g, int x, int y, int w, int h, int r, uint32_t c, int a);
void gfx_rrect_grad(gfx_t *g, int x, int y, int w, int h, int r,
                    uint32_t top, uint32_t bottom, int a);
void gfx_rrect_outline(gfx_t *g, int x, int y, int w, int h, int r, int thick,
                       uint32_t c, int a);
void gfx_circle(gfx_t *g, float cx, float cy, float r, uint32_t c, int a);
void gfx_ring(gfx_t *g, float cx, float cy, float r, float thick, uint32_t c, int a);
void gfx_glow(gfx_t *g, float cx, float cy, float r, uint32_t c, int a);

/* Text with the built-in 8x8 font. */
void gfx_glyph(gfx_t *g, int x, int y, char ch, uint32_t c, int sx, int sy, int a);
void gfx_text_a(gfx_t *g, int x, int y, const char *s, uint32_t c, int scale, int a);
void gfx_text(gfx_t *g, int x, int y, const char *s, uint32_t c, int scale);
int  gfx_text_width(const char *s, int scale);

/* The Astro logo: a shaded planet with a ring. tilt is in radians. */
void gfx_planet(gfx_t *g, float cx, float cy, float r, float tilt, int a);

#endif
