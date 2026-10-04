/*
 * Renders frames of Astro's graphical home screen to BMP files, without opening a
 * window. Handy for checking the UI (and for screenshots) on any machine.
 *
 *   gcc -std=gnu11 -O2 -DASTRO_HOSTED -Isrc/kernel -Isrc/desktop tools/preview.c \
 *       src/desktop/desktop.c src/desktop/gfx.c src/kernel/kprintf.c src/kernel/shell.c \
 *       src/kernel/vga.c src/kernel/bootlog.c -lm -o preview
 *   ./preview <output-dir> [width height [touch]]
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "desktop.h"
#include "keyboard.h"
#include "platform.h"
#include "sysinfo.h"
#include "timer.h"

uint32_t    g_mem_lower_kb = 640;
uint32_t    g_mem_upper_kb = 128 * 1024;
const char *g_platform_name = "Preview renderer";

static uint32_t clock_ms;

uint32_t timer_ticks(void) { return clock_ms / 10; }

void platform_local_time(platform_time_t *t)
{
    t->year = 2026; t->month = 10; t->day = 4; t->weekday = 0;
    t->hour = 10; t->minute = 42; t->second = (int)(clock_ms / 1000) % 60;
}

static int width = 1024, height = 640;
static uint32_t *pixels;
static const char *outdir = ".";
static int shot;

static void save(const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%02d-%s.bmp", outdir, ++shot, name);
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror(path);
        exit(1);
    }

    uint32_t row = ((uint32_t)width * 3 + 3) & ~3u, size = 54 + row * (uint32_t)height;
    uint8_t hdr[54] = { 'B', 'M' };
    memcpy(hdr + 2, &size, 4);
    uint32_t off = 54, dib = 40, planes_bpp = 1 | (24u << 16);
    memcpy(hdr + 10, &off, 4);
    memcpy(hdr + 14, &dib, 4);
    memcpy(hdr + 18, &width, 4);
    memcpy(hdr + 22, &height, 4);
    memcpy(hdr + 26, &planes_bpp, 4);
    fwrite(hdr, 1, 54, f);

    uint8_t *line = calloc(1, row);
    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            uint32_t p = pixels[y * width + x];
            line[x * 3 + 0] = (uint8_t)p;
            line[x * 3 + 1] = (uint8_t)(p >> 8);
            line[x * 3 + 2] = (uint8_t)(p >> 16);
        }
        fwrite(line, 1, row, f);
    }
    free(line);
    fclose(f);
    printf("wrote %s\n", path);
}

/* Advances the simulated clock in 16 ms steps and renders the last frame. */
static void run(uint32_t ms, const char *name)
{
    uint32_t end = clock_ms + ms;
    while (clock_ms < end) {
        clock_ms += 16;
        desktop_update(clock_ms);
    }
    desktop_draw(pixels, width, height);
    if (name)
        save(name);
}

static void type(const char *s)
{
    for (; *s; s++)
        desktop_key(*s == '\r' ? '\n' : *s);
}

static void click(int x, int y)
{
    desktop_mouse(DESKTOP_MOUSE_MOVE, x, y);
    desktop_mouse(DESKTOP_MOUSE_DOWN, x, y);
    desktop_mouse(DESKTOP_MOUSE_UP, x, y);
}

int main(int argc, char **argv)
{
    bool touch = false;

    if (argc > 1)
        outdir = argv[1];
    if (argc > 3) {
        width = atoi(argv[2]);
        height = atoi(argv[3]);
    }
    if (argc > 4)
        touch = true;

    pixels = calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    desktop_init(width, height);
    desktop_set_touch_mode(touch);

    run(0, NULL);
    run(800, "splash");
    run(2400, "home");

    type("t");                          /* typing on the home screen opens the launcher */
    run(500, "launcher-search-t");
    type("erm");
    run(300, "launcher-search-term");

    type("\r");                         /* open Terminal */
    run(500, "terminal-open");
    type("help\r");
    type("ver\r");
    type("echo hello from Astro\r");
    run(300, "terminal-help");

    type("exit\r");
    run(500, NULL);

    type("info\r");
    type("\r");                         /* leave the launcher search (empty = home) */
    desktop_key(27);
    run(300, NULL);
    type("inf");
    type("\r");
    run(500, "info");
    desktop_key(27);
    run(400, NULL);

    type("sett");
    type("\r");
    run(500, "settings");
    click(width / 2 + 120, height / 2 - 40);   /* a theme card */
    run(300, "settings-click");
    desktop_key(27);
    run(400, NULL);

    type("abo");
    type("\r");
    run(500, "about");
    desktop_key(27);
    run(400, "home-after");

    click(width - 24, 17);               /* power button */
    run(200, "power-menu");

    free(pixels);
    return 0;
}
