#include "gfx.h"

#include <math.h>
#include <string.h>

#include "font8x8.h"

static float clamp01(float v)
{
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

uint32_t gfx_mix(uint32_t a, uint32_t b, int t)
{
    if (t <= 0)
        return a;
    if (t >= 255)
        return b;

    uint32_t r  = ((a >> 16) & 255) * (uint32_t)(255 - t) + ((b >> 16) & 255) * (uint32_t)t;
    uint32_t gr = ((a >> 8) & 255)  * (uint32_t)(255 - t) + ((b >> 8) & 255)  * (uint32_t)t;
    uint32_t bl = (a & 255)         * (uint32_t)(255 - t) + (b & 255)         * (uint32_t)t;
    return ((r / 255) << 16) | ((gr / 255) << 8) | (bl / 255);
}

void gfx_clear(gfx_t *g, uint32_t c)
{
    for (int i = 0; i < g->w * g->h; i++)
        g->px[i] = c & 0xFFFFFF;
}

void gfx_put(gfx_t *g, int x, int y, uint32_t c, int a)
{
    if (a <= 0 || x < 0 || y < 0 || x >= g->w || y >= g->h)
        return;
    uint32_t *p = &g->px[y * g->w + x];
    *p = a >= 255 ? (c & 0xFFFFFF) : gfx_mix(*p, c, a);
}

void gfx_rect(gfx_t *g, int x, int y, int w, int h, uint32_t c, int a)
{
    int x0 = imax(x, 0), y0 = imax(y, 0);
    int x1 = imin(x + w, g->w), y1 = imin(y + h, g->h);

    for (int py = y0; py < y1; py++) {
        uint32_t *row = &g->px[py * g->w];
        if (a >= 255) {
            for (int px = x0; px < x1; px++)
                row[px] = c & 0xFFFFFF;
        } else if (a > 0) {
            for (int px = x0; px < x1; px++)
                row[px] = gfx_mix(row[px], c, a);
        }
    }
}

/* Coverage (0..1) of a rounded rectangle at a pixel center. */
static float rrect_cov(float px, float py, float cx, float cy, float hw, float hh, float r)
{
    float qx = fabsf(px - cx) - (hw - r);
    float qy = fabsf(py - cy) - (hh - r);
    float d;

    if (qx <= 0.0f && qy <= 0.0f)
        d = (qx > qy ? qx : qy) - r;
    else if (qy <= 0.0f)
        d = qx - r;
    else if (qx <= 0.0f)
        d = qy - r;
    else
        d = sqrtf(qx * qx + qy * qy) - r;

    return clamp01(0.5f - d);
}

static void rrect_impl(gfx_t *g, int x, int y, int w, int h, int r,
                       uint32_t top, uint32_t bottom, int a, int border)
{
    if (w <= 0 || h <= 0)
        return;
    if (r > imin(w, h) / 2)
        r = imin(w, h) / 2;

    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float hw = w * 0.5f, hh = h * 0.5f;
    float ir = (float)(r > border ? r - border : 0);

    for (int py = imax(y, 0); py < imin(y + h, g->h); py++) {
        uint32_t col = gfx_mix(top, bottom, (py - y) * 255 / h);
        for (int px = imax(x, 0); px < imin(x + w, g->w); px++) {
            float cov = rrect_cov(px + 0.5f, py + 0.5f, cx, cy, hw, hh, (float)r);
            if (border > 0) {
                cov -= rrect_cov(px + 0.5f, py + 0.5f, cx, cy, hw - border, hh - border, ir);
                if (cov < 0.0f)
                    cov = 0.0f;
            }
            gfx_put(g, px, py, col, (int)(cov * (float)a + 0.5f));
        }
    }
}

void gfx_rrect(gfx_t *g, int x, int y, int w, int h, int r, uint32_t c, int a)
{
    rrect_impl(g, x, y, w, h, r, c, c, a, 0);
}

void gfx_rrect_grad(gfx_t *g, int x, int y, int w, int h, int r,
                    uint32_t top, uint32_t bottom, int a)
{
    rrect_impl(g, x, y, w, h, r, top, bottom, a, 0);
}

void gfx_rrect_outline(gfx_t *g, int x, int y, int w, int h, int r, int thick,
                       uint32_t c, int a)
{
    rrect_impl(g, x, y, w, h, r, c, c, a, thick);
}

void gfx_circle(gfx_t *g, float cx, float cy, float r, uint32_t c, int a)
{
    int x0 = (int)floorf(cx - r) - 1, x1 = (int)ceilf(cx + r) + 1;
    int y0 = (int)floorf(cy - r) - 1, y1 = (int)ceilf(cy + r) + 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float cov = clamp01(r - sqrtf(dx * dx + dy * dy) + 0.5f);
            gfx_put(g, x, y, c, (int)(cov * (float)a + 0.5f));
        }
    }
}

void gfx_ring(gfx_t *g, float cx, float cy, float r, float thick, uint32_t c, int a)
{
    int x0 = (int)floorf(cx - r) - 1, x1 = (int)ceilf(cx + r) + 1;
    int y0 = (int)floorf(cy - r) - 1, y1 = (int)ceilf(cy + r) + 1;

    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float d = sqrtf(dx * dx + dy * dy);
            float cov = clamp01(r - d + 0.5f) - clamp01(r - thick - d + 0.5f);
            if (cov > 0.0f)
                gfx_put(g, x, y, c, (int)(cov * (float)a + 0.5f));
        }
    }
}

void gfx_glow(gfx_t *g, float cx, float cy, float r, uint32_t c, int a)
{
    int x0 = imax((int)(cx - r), 0), x1 = imin((int)(cx + r) + 1, g->w);
    int y0 = imax((int)(cy - r), 0), y1 = imin((int)(cy + r) + 1, g->h);

    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            float dx = x - cx, dy = y - cy;
            float d = sqrtf(dx * dx + dy * dy) / r;
            if (d < 1.0f) {
                float f = 1.0f - d;
                gfx_put(g, x, y, c, (int)(f * f * (float)a));
            }
        }
    }
}

void gfx_glyph(gfx_t *g, int x, int y, char ch, uint32_t c, int sx, int sy, int a)
{
    if (ch < 32 || ch > 126)
        return;

    const uint8_t *rows = font8x8[ch - 32];
    for (int r = 0; r < 8; r++) {
        uint8_t bits = rows[r];
        for (int col = 0; col < 8; col++)
            if (bits & (1u << col))
                gfx_rect(g, x + col * sx, y + r * sy, sx, sy, c, a);
    }
}

void gfx_text_a(gfx_t *g, int x, int y, const char *s, uint32_t c, int scale, int a)
{
    for (; *s; s++, x += 8 * scale)
        gfx_glyph(g, x, y, *s, c, scale, scale, a);
}

void gfx_text(gfx_t *g, int x, int y, const char *s, uint32_t c, int scale)
{
    gfx_text_a(g, x, y, s, c, scale, 255);
}

int gfx_text_width(const char *s, int scale)
{
    return (int)strlen(s) * 8 * scale;
}

/* ---- planet logo ---- */

void gfx_planet(gfx_t *g, float cx, float cy, float r, float tilt, int a)
{
    const float ct = cosf(tilt), st = sinf(tilt);
    const float ra = r * 1.75f, rb = r * 0.50f;          /* ring ellipse radii */
    const float soft = 1.5f / rb;                        /* ring edge softness */
    const int ext = (int)(r * 1.9f) + 2;

    for (int y = (int)cy - ext; y <= (int)cy + ext; y++) {
        for (int x = (int)cx - ext; x <= (int)cx + ext; x++) {
            if (x < 0 || y < 0 || x >= g->w || y >= g->h)
                continue;

            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float rx = dx * ct + dy * st;                /* ring-aligned frame */
            float ry = -dx * st + dy * ct;

            /* ring coverage and color */
            float e = sqrtf((rx / ra) * (rx / ra) + (ry / rb) * (ry / rb));
            float ring = 0.0f;
            uint32_t ring_col = 0;
            if (e > 0.55f && e < 1.1f) {
                float inner = (e - 0.62f) / soft, outer = (0.98f - e) / soft;
                ring = clamp01((inner < outer ? inner : outer) + 0.5f) * 0.92f;
                float stripe = 0.5f + 0.5f * sinf(e * 38.0f);
                uint32_t base = gfx_mix(0xFFB347, 0xFFF2A8, (int)(clamp01(rx / ra * 0.5f + 0.5f) * 255.0f));
                ring_col = gfx_mix(base, 0x8A5A18, (int)(stripe * 70.0f));
            }

            /* back part of the ring (drawn under the planet) */
            if (ry < 0.0f && ring > 0.0f)
                gfx_put(g, x, y, ring_col, (int)(ring * (float)a));

            /* planet body */
            float d = sqrtf(dx * dx + dy * dy);
            float cov = clamp01(r - d + 0.5f);
            if (cov > 0.0f) {
                float nx = dx / r, ny = dy / r;
                float nz = sqrtf(clamp01(1.0f - nx * nx - ny * ny));
                float lam = -0.45f * nx - 0.55f * ny + 0.70f * nz;
                lam = clamp01(lam);
                uint32_t col = lam < 0.55f
                    ? gfx_mix(0x2A1170, 0xC43FD8, (int)(lam / 0.55f * 255.0f))
                    : gfx_mix(0xC43FD8, 0xFF9BF0, (int)((lam - 0.55f) / 0.45f * 255.0f));
                gfx_put(g, x, y, col, (int)(cov * (float)a));
            }

            /* front part of the ring (drawn over the planet) */
            if (ry >= 0.0f && ring > 0.0f)
                gfx_put(g, x, y, ring_col, (int)(ring * (float)a));
        }
    }
}
