/* Astro's graphical home screen. See desktop.h for how a host drives it. */
#include "desktop.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gfx.h"
#include "keyboard.h"
#include "platform.h"
#include "power.h"
#include "shell.h"
#include "sysinfo.h"
#include "version.h"
#include "vga.h"

#define STATUS_H 34
#define CELL_W   136
#define CELL_H   130
#define TILE     80
#define BOOT_MS  2000
#define TITLE_H  36

#define WHITE 0xFFFFFF

typedef struct { int x, y, w, h; } rect_t;

/* ------------------------------------------------------------------ data */

typedef enum { APP_TERMINAL, APP_INFO, APP_SETTINGS, APP_ABOUT, APP_COUNT } app_id_t;

typedef struct {
    const char *name;       /* label under the icon */
    const char *keywords;   /* extra words matched by the launcher search */
    const char *title;      /* window title */
} app_def_t;

static const app_def_t apps[APP_COUNT] = {
    { "Terminal", "shell command console cli",       "Terminal" },
    { "Info",     "system information memory uptime", "System Info" },
    { "Settings", "theme appearance wallpaper clock", "Settings" },
    { "About",    "version astro os",                 "About Astro" },
};

typedef struct {
    const char *name;
    uint32_t top, bottom, glow1, glow2, accent;
} theme_t;

static const theme_t themes[3] = {
    { "Nebula", 0x0B0626, 0x2A1160, 0x7B2FF7, 0xF107A3, 0xC060FF },
    { "Ocean",  0x04101F, 0x0B4C6E, 0x00B4D8, 0x48CAE4, 0x35C9E8 },
    { "Sunset", 0x1A0B2E, 0x7A2548, 0xFF6B6B, 0xFFB347, 0xFF8A5C },
};

/* Standard VGA text-mode palette, used by the terminal window. */
static const uint32_t vga_rgb[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static const char *const weekdays[7] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
static const char *const months[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

/* ----------------------------------------------------------------- state */

static int W, H;
static uint32_t now, last_now, boot_start, session_start;
static bool boot_pending = true;
static bool dirty = true;
static uint32_t last_half;

static bool touch_mode;
static int mx = -1, my = -1, press_x, press_y;
static bool mdown;
static bool click_pending;
static int click_x, click_y;
static int top_layer;       /* 0 home, 1 launcher, 2 app window */

static int theme;
static bool show_seconds;
static gfx_t wall;
static int wall_theme = -1;

static bool quit_req, reboot_req;

static bool launcher_open;
static float launcher_anim;
static char search[24];
static int search_len;

static bool app_open;
static int open_app = APP_TERMINAL;
static float app_anim;
static bool power_menu;

static bool term_started;
static bool kb_shift;

/* Terminal screen: the VGA text layer renders into this buffer (see vga.h hooks). */
static uint16_t term_buf[VGA_WIDTH * VGA_HEIGHT];
static int term_cx, term_cy;
static bool term_cursor = true;

volatile uint16_t *vga_hw_buffer(void) { return term_buf; }
void vga_hw_set_cursor(int x, int y) { term_cx = x; term_cy = y; }
void vga_hw_cursor_enable(bool enable) { term_cursor = enable; }

/* Power requests only set a flag; the host loop acts on it. */
void power_reboot(void)   { reboot_req = true; }
void power_shutdown(void) { quit_req = true; }

/* --------------------------------------------------------------- helpers */

static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

static float ease(float t)
{
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

static bool in_rect(int px, int py, int x, int y, int w, int h)
{
    return px >= x && py >= y && px < x + w && py < y + h;
}

/* Consumes the pending click if it lands in the rectangle of the active layer. */
static bool hit(int layer, int x, int y, int w, int h)
{
    if (!click_pending || layer != top_layer || !in_rect(click_x, click_y, x, y, w, h))
        return false;
    click_pending = false;
    return true;
}

static bool hovered(int layer, int x, int y, int w, int h)
{
    return !touch_mode && layer == top_layer && in_rect(mx, my, x, y, w, h);
}

static bool pressed(int layer, int x, int y, int w, int h)
{
    return mdown && layer == top_layer && in_rect(press_x, press_y, x, y, w, h) &&
           in_rect(mx, my, x, y, w, h);
}

static bool ci_contains(const char *hay, const char *needle)
{
    size_t n = strlen(needle), h = strlen(hay);
    if (n == 0)
        return true;
    for (size_t i = 0; i + n <= h; i++) {
        size_t j = 0;
        while (j < n) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b)
                break;
            j++;
        }
        if (j == n)
            return true;
    }
    return false;
}

static void open_app_id(int id)
{
    if (id == APP_TERMINAL && !term_started) {
        vga_init();
        shell_start();
        term_started = true;
    }
    open_app = id;
    app_open = true;
    launcher_open = false;
    kb_shift = false;
    dirty = true;
}

static void close_app(void)
{
    app_open = false;
    dirty = true;
}

static void toggle_launcher(void)
{
    launcher_open = !launcher_open;
    search_len = 0;
    search[0] = '\0';
    dirty = true;
}

static void terminal_input(int key)
{
    dirty = true;
    if (shell_key(key)) {
        term_started = false;
        close_app();
    }
}

static int filter_apps(int *out)
{
    int n = 0;
    for (int i = 0; i < APP_COUNT; i++)
        if (ci_contains(apps[i].name, search) || ci_contains(apps[i].keywords, search))
            out[n++] = i;
    return n;
}

/* ------------------------------------------------------------- wallpaper */

static uint32_t rng_state;

static uint32_t rnd(void)
{
    rng_state = rng_state * 1664525u + 1013904223u;
    return rng_state >> 8;
}

static void render_wallpaper(void)
{
    free(wall.px);
    wall.w = W;
    wall.h = H;
    wall.px = malloc((size_t)W * (size_t)H * sizeof(uint32_t));
    wall_theme = theme;
    if (!wall.px)
        return;

    const theme_t *t = &themes[theme];
    int ms = imin(W, H);

    for (int y = 0; y < H; y++) {
        uint32_t row = gfx_mix(t->top, t->bottom, y * 255 / imax(H - 1, 1));
        for (int x = 0; x < W; x++)
            wall.px[y * W + x] = row;
    }

    gfx_glow(&wall, W * 0.18f, H * 0.22f, ms * 0.75f, t->glow1, 80);
    gfx_glow(&wall, W * 0.86f, H * 0.72f, ms * 0.85f, t->glow2, 70);

    rng_state = 12345u;
    int stars = W * H / 4500;
    for (int i = 0; i < stars; i++) {
        int x = (int)(rnd() % (uint32_t)W), y = (int)(rnd() % (uint32_t)H);
        int b = 110 + (int)(rnd() % 146u);
        int size = (rnd() % 10u == 0) ? 2 : 1;
        gfx_rect(&wall, x, y, size, size, WHITE, b);
        if (rnd() % 28u == 0) {                 /* a few sparkles */
            gfx_rect(&wall, x - 4, y, 9, 1, WHITE, b / 3);
            gfx_rect(&wall, x, y - 4, 1, 9, WHITE, b / 3);
        }
    }

    /* the big planet, dimmed so it does not fight with the icons */
    float pr, pcx, pcy;
    if (W >= H) {
        pr = H * 0.24f; pcx = W * 0.83f; pcy = H * 0.64f;
    } else {
        pr = W * 0.26f; pcx = W * 0.74f; pcy = H * 0.80f;
    }
    gfx_planet(&wall, pcx, pcy, pr, -0.42f, 150);
}

/* ----------------------------------------------------------------- icons */

static void draw_tile(gfx_t *g, int id, int x, int y, int s, bool is_pressed, bool is_hover)
{
    uint32_t top, bot;
    switch (id) {
    case APP_TERMINAL: top = 0x2B3A5C; bot = 0x0B1220; break;
    case APP_INFO:     top = 0x3B82D6; bot = 0x1A3A70; break;
    case APP_SETTINGS: top = 0x8B5CF6; bot = 0x44337A; break;
    case APP_ABOUT:    top = 0x2A1160; bot = 0x0B0626; break;
    default:           top = 0x9B4DFF; bot = 0x3A1B9A; break;   /* Astro launcher */
    }
    int r = s / 4;
    int sc = imax(s / 20, 1);

    gfx_rrect(g, x + 2, y + 5, s, s, r, 0x000000, 80);
    gfx_rrect_grad(g, x, y, s, s, r, top, bot, 255);

    switch (id) {
    case APP_TERMINAL:
        gfx_text(g, x + (s - 16 * sc) / 2, y + (s - 8 * sc) / 2, ">_", 0x7CFFB2, sc);
        break;
    case APP_INFO:
        gfx_circle(g, x + s / 2.0f, y + s / 2.0f, s * 0.30f, WHITE, 255);
        gfx_text(g, x + (s - 8 * sc) / 2, y + (s - 8 * sc) / 2, "i", bot, sc);
        break;
    case APP_SETTINGS: {
        float cx = x + s / 2.0f, cy = y + s / 2.0f;
        for (int k = 0; k < 8; k++) {
            float a = (float)k * 0.785398f;
            gfx_circle(g, cx + cosf(a) * s * 0.29f, cy + sinf(a) * s * 0.29f, s * 0.075f, WHITE, 255);
        }
        gfx_circle(g, cx, cy, s * 0.25f, WHITE, 255);
        gfx_circle(g, cx, cy, s * 0.10f, gfx_mix(top, bot, 150), 255);
        break;
    }
    default:
        gfx_planet(g, x + s / 2.0f, y + s / 2.0f, s * 0.19f, -0.42f, 255);
        break;
    }

    if (is_hover)
        gfx_rrect_outline(g, x, y, s, s, r, 2, themes[theme].accent, 255);
    if (is_pressed)
        gfx_rrect(g, x, y, s, s, r, 0x000000, 70);
}

/* Draws a centered grid of app tiles and handles clicks on them. */
static void draw_app_grid(gfx_t *g, const int *list, int count, int top, int layer)
{
    int cols = imax(1, imin(4, (W - 40) / CELL_W));
    int rows = (count + cols - 1) / cols;

    for (int i = 0; i < count; i++) {
        int col = i % cols, row = i / cols;
        int in_row = (row == rows - 1 && count % cols) ? count % cols : cols;
        int x0 = (W - in_row * CELL_W) / 2 + col * CELL_W;
        int y0 = top + row * CELL_H;
        int id = list[i];

        draw_tile(g, id, x0 + (CELL_W - TILE) / 2, y0, TILE,
                  pressed(layer, x0, y0, CELL_W, CELL_H),
                  hovered(layer, x0, y0, CELL_W, CELL_H));

        int lw = gfx_text_width(apps[id].name, 2);
        gfx_text(g, x0 + (CELL_W - lw) / 2 + 1, y0 + TILE + 13, apps[id].name, 0x000000, 2);
        gfx_text(g, x0 + (CELL_W - lw) / 2, y0 + TILE + 12, apps[id].name, WHITE, 2);

        if (hit(layer, x0, y0, CELL_W, CELL_H))
            open_app_id(id);
    }
}

/* ------------------------------------------------------------------ dock */

#define DOCK_ITEMS 3
static const int dock_apps[DOCK_ITEMS] = { APP_TERMINAL, -1, APP_SETTINGS };

static void dock_geometry(rect_t *panel, rect_t items[DOCK_ITEMS])
{
    const int ts = 60, gap = 18, pad = 14;
    panel->w = DOCK_ITEMS * ts + (DOCK_ITEMS - 1) * gap + 2 * pad;
    panel->h = ts + 2 * pad;
    panel->x = (W - panel->w) / 2;
    panel->y = H - panel->h - 14;
    for (int i = 0; i < DOCK_ITEMS; i++) {
        items[i].x = panel->x + pad + i * (ts + gap);
        items[i].y = panel->y + pad;
        items[i].w = ts;
        items[i].h = ts;
    }
}

static void draw_dock(gfx_t *g)
{
    rect_t p, it[DOCK_ITEMS];
    dock_geometry(&p, it);

    gfx_rrect(g, p.x, p.y, p.w, p.h, 26, 0x0E0A24, 175);
    gfx_rrect_outline(g, p.x, p.y, p.w, p.h, 26, 1, WHITE, 40);

    for (int i = 0; i < DOCK_ITEMS; i++) {
        bool dock_live = top_layer <= 1;
        bool inside = in_rect(mx, my, it[i].x, it[i].y, it[i].w, it[i].h);
        draw_tile(g, dock_apps[i], it[i].x, it[i].y, it[i].w,
                  dock_live && mdown && inside && in_rect(press_x, press_y, it[i].x, it[i].y, it[i].w, it[i].h),
                  dock_live && !touch_mode && inside);
    }
}

/* ---------------------------------------------------- status bar and menu */

static rect_t power_button(void)
{
    rect_t r = { W - 40, 4, 32, 26 };
    return r;
}

static rect_t power_menu_rect(void)
{
    rect_t r = { W - 196, STATUS_H + 2, 188, 92 };
    return r;
}

static void process_global_input(void)
{
    rect_t pb = power_button();

    if (click_pending && in_rect(click_x, click_y, pb.x, pb.y, pb.w, pb.h)) {
        power_menu = !power_menu;
        click_pending = false;
    }

    if (power_menu && click_pending) {
        rect_t m = power_menu_rect();
        for (int i = 0; i < 2; i++) {
            if (in_rect(click_x, click_y, m.x, m.y + 4 + i * 44, m.w, 44)) {
                click_pending = false;
                power_menu = false;
                if (i == 0)
                    power_reboot();
                else
                    power_shutdown();
                return;
            }
        }
        click_pending = false;      /* clicked elsewhere: just dismiss the menu */
        power_menu = false;
    }

    if (click_pending && top_layer <= 1 && app_anim <= 0.01f) {
        rect_t p, it[DOCK_ITEMS];
        dock_geometry(&p, it);
        for (int i = 0; i < DOCK_ITEMS; i++) {
            if (in_rect(click_x, click_y, it[i].x, it[i].y, it[i].w, it[i].h)) {
                click_pending = false;
                if (dock_apps[i] < 0)
                    toggle_launcher();
                else
                    open_app_id(dock_apps[i]);
                break;
            }
        }
    }
}

static void draw_status(gfx_t *g, const platform_time_t *pt)
{
    char buf[16];

    gfx_rect(g, 0, 0, W, STATUS_H, 0x000000, 110);
    gfx_planet(g, 21, 17, 8, -0.42f, 255);
    gfx_text(g, 42, 9, "Astro", WHITE, 2);

    snprintf(buf, sizeof buf, "%02d:%02d", pt->hour, pt->minute);
    gfx_text(g, W - 52 - gfx_text_width(buf, 2), 9, buf, WHITE, 2);

    rect_t pb = power_button();
    bool hov = hovered(top_layer, pb.x, pb.y, pb.w, pb.h) || (!touch_mode && in_rect(mx, my, pb.x, pb.y, pb.w, pb.h));
    if (hov || power_menu)
        gfx_rrect(g, pb.x, pb.y, pb.w, pb.h, 8, WHITE, 40);
    gfx_ring(g, pb.x + pb.w / 2.0f, pb.y + pb.h / 2.0f, 7.5f, 2.0f, WHITE, 255);
    gfx_rect(g, pb.x + pb.w / 2 - 1, pb.y + 5, 2, 8, WHITE, 255);
}

static void draw_power_menu(gfx_t *g)
{
    static const char *const labels[2] = { "Restart", "Shut down" };
    rect_t m = power_menu_rect();

    gfx_rrect(g, m.x + 2, m.y + 4, m.w, m.h, 14, 0x000000, 90);
    gfx_rrect(g, m.x, m.y, m.w, m.h, 14, 0x1B1440, 250);
    gfx_rrect_outline(g, m.x, m.y, m.w, m.h, 14, 1, WHITE, 50);

    for (int i = 0; i < 2; i++) {
        int iy = m.y + 4 + i * 44;
        if (!touch_mode && in_rect(mx, my, m.x, iy, m.w, 44))
            gfx_rrect(g, m.x + 6, iy + 2, m.w - 12, 40, 10, themes[theme].accent, 90);
        gfx_text(g, m.x + 24, iy + 14, labels[i], WHITE, 2);
    }
}

/* ------------------------------------------------------------ home screen */

static void draw_home(gfx_t *g, const platform_time_t *pt)
{
    char buf[32];
    int sc = show_seconds ? 6 : 8;
    int y = STATUS_H + imax(H / 12, 20);

    if (show_seconds)
        snprintf(buf, sizeof buf, "%02d:%02d:%02d", pt->hour, pt->minute, pt->second);
    else
        snprintf(buf, sizeof buf, "%02d:%02d", pt->hour, pt->minute);

    int tw = gfx_text_width(buf, sc);
    gfx_text_a(g, (W - tw) / 2 + 3, y + 3, buf, 0x000000, sc, 100);
    gfx_text(g, (W - tw) / 2, y, buf, WHITE, sc);

    snprintf(buf, sizeof buf, "%s, %s %d", weekdays[pt->weekday % 7],
             months[(pt->month + 11) % 12], pt->day);
    int dy = y + sc * 8 + 14;
    gfx_text(g, (W - gfx_text_width(buf, 2)) / 2, dy, buf, 0xD8C8FF, 2);

    int all[APP_COUNT];
    for (int i = 0; i < APP_COUNT; i++)
        all[i] = i;
    draw_app_grid(g, all, APP_COUNT, dy + 56, 0);
}

/* --------------------------------------------------------------- launcher */

static void draw_pill(gfx_t *g, int x, int y, int w, int h, const char *label, bool hov, bool prs)
{
    gfx_rrect(g, x, y, w, h, h / 2, prs ? 0x2A2060 : (hov ? 0x3A2D80 : 0x1F1850), 255);
    gfx_rrect_outline(g, x, y, w, h, h / 2, 1, WHITE, 50);
    gfx_text(g, x + (w - gfx_text_width(label, 2)) / 2, y + (h - 16) / 2, label, WHITE, 2);
}

static void draw_launcher(gfx_t *g)
{
    float t = ease(launcher_anim);
    int ly = STATUS_H;

    gfx_rect(g, 0, ly, W, H - ly, 0x07041A, (int)(t * 235.0f));
    if (t < 0.5f)
        return;

    /* search bar */
    int bw = imin(W - 60, 520), bh = 44;
    int bx = (W - bw) / 2, by = ly + 24;
    gfx_rrect(g, bx, by, bw, bh, 22, 0x1B1440, 255);
    gfx_rrect_outline(g, bx, by, bw, bh, 22, 2, themes[theme].accent, 255);
    if (search_len == 0) {
        gfx_text(g, bx + 20, by + 14, "Search apps...", 0x8A7FB8, 2);
    } else {
        gfx_text(g, bx + 20, by + 14, search, WHITE, 2);
        if ((now / 500) % 2 == 0)
            gfx_rect(g, bx + 20 + search_len * 16, by + 11, 2, 22, WHITE, 255);
    }
    hit(1, bx, by, bw, bh);         /* clicking the bar must not close the launcher */

    int list[APP_COUNT];
    int n = filter_apps(list);
    int grid_top = by + bh + 36;
    if (n == 0)
        gfx_text(g, (W - gfx_text_width("No apps found", 2)) / 2, grid_top + 20, "No apps found", 0x8A7FB8, 2);
    else
        draw_app_grid(g, list, n, grid_top, 1);

    /* power shortcuts above the dock */
    rect_t p, it[DOCK_ITEMS];
    dock_geometry(&p, it);
    int py = p.y - 58, pw = 150, ph = 40;
    int px0 = W / 2 - pw - 10, px1 = W / 2 + 10;
    draw_pill(g, px0, py, pw, ph, "Restart",
              hovered(1, px0, py, pw, ph), pressed(1, px0, py, pw, ph));
    draw_pill(g, px1, py, pw, ph, "Shut down",
              hovered(1, px1, py, pw, ph), pressed(1, px1, py, pw, ph));
    if (hit(1, px0, py, pw, ph))
        power_reboot();
    if (hit(1, px1, py, pw, ph))
        power_shutdown();

    /* a click on empty space closes the launcher */
    if (launcher_open && click_pending) {
        click_pending = false;
        launcher_open = false;
        dirty = true;
    }
}

/* ------------------------------------------------------------ app window */

static int content_height(void)
{
    switch (open_app) {
    case APP_TERMINAL: return touch_mode ? 604 : 418;   /* + on-screen keyboard on touch */
    case APP_INFO:     return 300;
    case APP_SETTINGS: return 330;
    default:           return 390;
    }
}

static rect_t window_rect(void)
{
    rect_t r;
    r.w = imin(W - 16, open_app == APP_TERMINAL ? 680 : 640);
    r.h = imin(H - STATUS_H - 16, TITLE_H + content_height());
    r.x = (W - r.w) / 2;
    r.y = STATUS_H + (H - STATUS_H - r.h) / 2;
    return r;
}

static void draw_key(gfx_t *g, int x, int y, int w, int h, const char *label, bool prs)
{
    gfx_rrect(g, x, y, w, h, 8, prs ? 0x5B4CB0 : 0x2B2257, 255);
    gfx_text(g, x + (w - gfx_text_width(label, 2)) / 2, y + (h - 16) / 2, label, WHITE, 2);
}

/* Touch devices have no physical keyboard: the terminal gets an on-screen one. */
static void draw_soft_keyboard(gfx_t *g, int x, int y, int w)
{
    static const char *const rows[4] = { "1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm" };
    const int kh = 32, gap = 4;
    int kw = (w - 20 - 9 * gap) / 10;

    for (int r = 0; r < 4; r++) {
        int n = (int)strlen(rows[r]);
        int rw = n * kw + (n - 1) * gap;
        int rx = x + (w - rw) / 2, ry = y + r * (kh + gap);
        for (int i = 0; i < n; i++) {
            int kx = rx + i * (kw + gap);
            char c = rows[r][i];
            if (kb_shift && c >= 'a' && c <= 'z')
                c = (char)(c - 32);
            char label[2] = { c, '\0' };
            draw_key(g, kx, ry, kw, kh, label, pressed(2, kx, ry, kw, kh));
            if (hit(2, kx, ry, kw, kh)) {
                terminal_input(c);
                kb_shift = false;
            }
        }
    }

    /* bottom row: Shift (2 units), Space (4), Bksp (2), Enter (2) */
    static const struct { const char *label; int units; } bottom[4] = {
        { "Shift", 2 }, { "Space", 4 }, { "Bksp", 2 }, { "Enter", 2 },
    };
    int ry = y + 4 * (kh + gap);
    int kx = x + 10;
    for (int i = 0; i < 4; i++) {
        int bw = bottom[i].units * kw + (bottom[i].units - 1) * gap;
        draw_key(g, kx, ry, bw, kh, bottom[i].label,
                 pressed(2, kx, ry, bw, kh) || (i == 0 && kb_shift));
        if (hit(2, kx, ry, bw, kh)) {
            if (i == 0)      kb_shift = !kb_shift;
            else if (i == 1) terminal_input(' ');
            else if (i == 2) terminal_input('\b');
            else             terminal_input('\n');
        }
        kx += bw + gap;
    }
}

static void draw_terminal(gfx_t *g, int cx, int cy, int cw)
{
    int tx = cx + (cw - VGA_WIDTH * 8) / 2, ty = cy + 6;

    gfx_rect(g, tx - 6, ty - 6, VGA_WIDTH * 8 + 12, VGA_HEIGHT * 16 + 12, 0x070710, 255);

    for (int row = 0; row < VGA_HEIGHT; row++) {
        for (int col = 0; col < VGA_WIDTH; col++) {
            uint16_t cell = term_buf[row * VGA_WIDTH + col];
            int attr = cell >> 8, ch = cell & 0xFF;
            int px = tx + col * 8, py = ty + row * 16;
            if ((attr >> 4) & 0x0F)
                gfx_rect(g, px, py, 8, 16, vga_rgb[(attr >> 4) & 0x0F], 255);
            if (ch > ' ' && ch < 127)
                gfx_glyph(g, px, py, (char)ch, vga_rgb[attr & 0x0F], 1, 2, 255);
        }
    }

    if (term_cursor && (now / 500) % 2 == 0)
        gfx_rect(g, tx + term_cx * 8, ty + term_cy * 16 + 13, 8, 3, 0xAAAAAA, 255);

    if (touch_mode)
        draw_soft_keyboard(g, cx, ty + VGA_HEIGHT * 16 + 10, cw);
}

static void draw_row(gfx_t *g, int x, int y, const char *label, const char *value)
{
    gfx_text(g, x, y, label, 0x8A7FB8, 2);
    gfx_text(g, x + 200, y, value, WHITE, 2);
}

static void draw_info(gfx_t *g, int cx, int cy, int cw)
{
    char buf[64];
    uint32_t up = (now - session_start) / 1000;
    int x = cx + 28, y = cy + 30;

    gfx_text(g, x, y, "System information", themes[theme].accent, 2);
    y += 48;

    snprintf(buf, sizeof buf, "%s OS %s", ASTRO_NAME, ASTRO_VERSION);
    draw_row(g, x, y, "System", buf);            y += 36;
    draw_row(g, x, y, "Platform", g_platform_name); y += 36;
    snprintf(buf, sizeof buf, "%d x %d", W, H);
    draw_row(g, x, y, "Display", buf);            y += 36;
    snprintf(buf, sizeof buf, "%u MiB", g_mem_upper_kb / 1024);
    draw_row(g, x, y, "Memory", buf);             y += 36;
    snprintf(buf, sizeof buf, "%02u:%02u:%02u", up / 3600, (up / 60) % 60, up % 60);
    draw_row(g, x, y, "Uptime", buf);

    gfx_planet(g, cx + cw - 110, cy + 130, 44, -0.42f, 255);
}

static void draw_settings(gfx_t *g, int cx, int cy, int cw)
{
    int x = cx + 28, y = cy + 30;

    gfx_text(g, x, y, "Appearance", themes[theme].accent, 2);
    gfx_text(g, x, y + 40, "Theme", 0x8A7FB8, 2);

    int gap = 16, card_w = (cw - 56 - 2 * gap) / 3, card_h = 120;
    for (int i = 0; i < 3; i++) {
        int kx = x + i * (card_w + gap), ky = y + 76;
        gfx_rrect_grad(g, kx, ky, card_w, card_h, 14, themes[i].top, themes[i].bottom, 255);
        gfx_circle(g, kx + card_w * 0.8f, ky + card_h * 0.4f, 18, themes[i].glow1, 200);
        gfx_text(g, kx + 14, ky + card_h - 28, themes[i].name, WHITE, 2);
        if (i == theme)
            gfx_rrect_outline(g, kx - 2, ky - 2, card_w + 4, card_h + 4, 16, 3, WHITE, 255);
        else if (hovered(2, kx, ky, card_w, card_h))
            gfx_rrect_outline(g, kx, ky, card_w, card_h, 14, 2, themes[i].accent, 255);
        if (hit(2, kx, ky, card_w, card_h)) {
            theme = i;
            wall_theme = -1;
        }
    }

    int ty = y + 76 + card_h + 40;
    gfx_text(g, x, ty + 6, "Show seconds on the home clock", WHITE, 2);
    int sw = 60, sh = 30, sx = cx + cw - 28 - sw;
    gfx_rrect(g, sx, ty, sw, sh, 15, show_seconds ? themes[theme].accent : 0x3A3360, 255);
    gfx_circle(g, show_seconds ? sx + sw - 15.0f : sx + 15.0f, ty + 15.0f, 11, WHITE, 255);
    if (hit(2, sx - 20, ty - 8, sw + 40, sh + 16))
        show_seconds = !show_seconds;
}

static void draw_about(gfx_t *g, int cx, int cy, int cw)
{
    char buf[64];
    int mid = cx + cw / 2;

    gfx_planet(g, (float)mid, (float)(cy + 120), 62, -0.42f, 255);

    gfx_text(g, mid - gfx_text_width("ASTRO OS", 4) / 2, cy + 226, "ASTRO OS", WHITE, 4);
    snprintf(buf, sizeof buf, "Version %s", ASTRO_VERSION);
    gfx_text(g, mid - gfx_text_width(buf, 2) / 2, cy + 276, buf, themes[theme].accent, 2);
    gfx_text(g, mid - gfx_text_width("An operating system written in C.", 2) / 2, cy + 316,
             "An operating system written in C.", 0xD8C8FF, 2);
    snprintf(buf, sizeof buf, "Running on: %s", g_platform_name);
    gfx_text(g, mid - gfx_text_width(buf, 2) / 2, cy + 348, buf, 0x8A7FB8, 2);
}

static void draw_app_window(gfx_t *g)
{
    float t = ease(app_anim);
    rect_t r = window_rect();
    int yo = (int)((1.0f - t) * 40.0f);
    int x = r.x, y = r.y + yo;

    gfx_rect(g, 0, STATUS_H, W, H - STATUS_H, 0x000000, (int)(t * 150.0f));

    gfx_rrect(g, x + 2, y + 8, r.w, r.h, 16, 0x000000, 90);
    gfx_rrect(g, x, y, r.w, r.h, 16, 0x15112E, 255);
    gfx_rrect(g, x, y, r.w, TITLE_H, 16, 0x241B4A, 255);           /* rounded top corners */
    gfx_rect(g, x, y + 16, r.w, TITLE_H - 16, 0x241B4A, 255);       /* square bottom edge */
    gfx_rect(g, x, y + TITLE_H, r.w, 1, WHITE, 30);
    gfx_text(g, x + 18, y + 10, apps[open_app].title, WHITE, 2);

    int bx = x + r.w - 26, by = y + TITLE_H / 2;
    bool hov = hovered(2, bx - 14, by - 14, 28, 28);
    gfx_circle(g, (float)bx, (float)by, 10, hov ? 0xFF8A80 : 0xFF5F57, 255);
    gfx_text(g, bx - 4, by - 4, "x", 0x4A0F0B, 1);
    if (hit(2, bx - 14, by - 14, 28, 28))
        close_app();

    int cx = x, cy = y + TITLE_H, cw = r.w;
    switch (open_app) {
    case APP_TERMINAL: draw_terminal(g, cx, cy, cw); break;
    case APP_INFO:     draw_info(g, cx, cy, cw);     break;
    case APP_SETTINGS: draw_settings(g, cx, cy, cw); break;
    default:           draw_about(g, cx, cy, cw);    break;
    }
}

/* ---------------------------------------------------------- boot splash */

static void draw_splash(gfx_t *g)
{
    uint32_t el = now - boot_start;
    int alpha = el < BOOT_MS - 400 ? 255 : (int)(255 * (BOOT_MS - el) / 400);
    if (alpha <= 0)
        return;

    gfx_rect(g, 0, 0, W, H, 0x05030F, alpha);
    if (alpha < 40)
        return;

    int cx = W / 2, cy = H / 2 - 40;
    gfx_planet(g, (float)cx, (float)cy, 46, -0.42f, alpha);
    gfx_text_a(g, cx - gfx_text_width("ASTRO", 5) / 2, cy + 100, "ASTRO", WHITE, 5, alpha);

    int bw = 220, bx = cx - bw / 2, by = cy + 168;
    float prog = (float)el / (float)(BOOT_MS - 400);
    if (prog > 1.0f)
        prog = 1.0f;
    gfx_rrect(g, bx, by, bw, 6, 3, 0x2A2060, alpha);
    gfx_rrect(g, bx, by, (int)(bw * prog), 6, 3, themes[theme].accent, alpha);

    const char *msg = prog < 0.35f ? "Loading kernel" : (prog < 0.7f ? "Starting services" : "Preparing home screen");
    gfx_text_a(g, cx - gfx_text_width(msg, 1) / 2, by + 20, msg, 0x8A7FB8, 1, alpha);
}

/* ------------------------------------------------------------ public API */

void desktop_init(int w, int h)
{
    W = w;
    H = h;
    boot_pending = true;
    launcher_open = false;
    launcher_anim = 0.0f;
    app_open = false;
    app_anim = 0.0f;
    power_menu = false;
    term_started = false;
    search_len = 0;
    search[0] = '\0';
    click_pending = false;
    mdown = false;
    quit_req = false;
    reboot_req = false;
    wall_theme = -1;
    dirty = true;
}

void desktop_resize(int w, int h)
{
    W = w;
    H = h;
    wall_theme = -1;
    dirty = true;
}

void desktop_set_touch_mode(bool touch)
{
    touch_mode = touch;
}

void desktop_mouse(int kind, int x, int y)
{
    dirty = true;
    mx = x;
    my = y;

    if (kind == DESKTOP_MOUSE_DOWN) {
        mdown = true;
        press_x = x;
        press_y = y;
    } else if (kind == DESKTOP_MOUSE_UP) {
        if (mdown) {
            int dx = x - press_x, dy = y - press_y;
            if (dx * dx + dy * dy <= 30 * 30) {
                click_pending = true;
                click_x = x;
                click_y = y;
            }
        }
        mdown = false;
        if (touch_mode) {
            mx = -1;
            my = -1;
        }
    }
}

void desktop_key(int key)
{
    dirty = true;

    if (power_menu) {
        if (key == 27)
            power_menu = false;
        return;
    }

    if (launcher_open) {
        int list[APP_COUNT];
        if (key == 27) {
            launcher_open = false;
        } else if (key == '\n') {
            if (filter_apps(list) > 0)
                open_app_id(list[0]);
        } else if (key == '\b') {
            if (search_len > 0)
                search[--search_len] = '\0';
        } else if (key >= ' ' && key < 127 && search_len < (int)sizeof search - 1) {
            search[search_len++] = (char)key;
            search[search_len] = '\0';
        }
        return;
    }

    if (app_open) {
        if (open_app == APP_TERMINAL)
            terminal_input(key);
        else if (key == 27)
            close_app();
        return;
    }

    /* on the home screen, typing starts a launcher search */
    if (key == '\n') {
        toggle_launcher();
    } else if (key > ' ' && key < 127) {
        toggle_launcher();
        search[search_len++] = (char)key;
        search[search_len] = '\0';
    }
}

bool desktop_handle_back(void)
{
    dirty = true;
    if (power_menu)    { power_menu = false;    return true; }
    if (launcher_open) { launcher_open = false; return true; }
    if (app_open)      { close_app();           return true; }
    return false;
}

bool desktop_update(uint32_t now_ms)
{
    if (boot_pending) {
        boot_pending = false;
        boot_start = now_ms;
        session_start = now_ms;
        last_now = now_ms;
        dirty = true;
    }
    now = now_ms;

    float dt = (float)(now - last_now);
    last_now = now;

    bool animating = false;
    float lt = launcher_open ? 1.0f : 0.0f;
    if (launcher_anim != lt) {
        launcher_anim += (lt > launcher_anim ? 1.0f : -1.0f) * dt / 180.0f;
        if ((lt > 0.5f && launcher_anim > 1.0f) || (lt < 0.5f && launcher_anim < 0.0f))
            launcher_anim = lt;
        animating = true;
    }
    float at = app_open ? 1.0f : 0.0f;
    if (app_anim != at) {
        app_anim += (at > app_anim ? 1.0f : -1.0f) * dt / 200.0f;
        if ((at > 0.5f && app_anim > 1.0f) || (at < 0.5f && app_anim < 0.0f))
            app_anim = at;
        animating = true;
    }
    if (now - boot_start < BOOT_MS)
        animating = true;

    uint32_t half = now / 500;
    bool tick = half != last_half;
    last_half = half;

    bool need = dirty || animating || tick;
    dirty = false;
    return need;
}

void desktop_draw(uint32_t *pixels, int w, int h)
{
    if (w != W || h != H)
        desktop_resize(w, h);

    gfx_t g = { pixels, w, h };

    if (wall_theme != theme || !wall.px || wall.w != w || wall.h != h)
        render_wallpaper();
    if (wall.px)
        memcpy(pixels, wall.px, (size_t)w * (size_t)h * sizeof(uint32_t));
    else
        gfx_clear(&g, 0x0B0626);

    platform_time_t pt;
    platform_local_time(&pt);

    bool booting = now - boot_start < BOOT_MS;
    if (booting)
        click_pending = false;

    top_layer = app_open ? 2 : (launcher_open ? 1 : 0);
    process_global_input();

    if (launcher_anim < 0.99f)          /* hidden behind the launcher once it is fully open */
        draw_home(&g, &pt);
    if (launcher_anim > 0.01f)
        draw_launcher(&g);
    if (app_anim <= 0.01f)
        draw_dock(&g);
    else
        draw_app_window(&g);
    draw_status(&g, &pt);
    if (power_menu)
        draw_power_menu(&g);
    if (booting)
        draw_splash(&g);

    click_pending = false;
}

bool desktop_quit_requested(void)
{
    return quit_req;
}

bool desktop_reboot_requested(void)
{
    bool r = reboot_req;
    reboot_req = false;
    return r;
}
