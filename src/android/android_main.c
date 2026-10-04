/* Astro OS for Android: a NativeActivity host for the graphical home screen. */
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "desktop.h"
#include "keyboard.h"
#include "platform.h"
#include "sysinfo.h"
#include "timer.h"

#define TAG "AstroOS"
#define LOG(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

/* The UI is laid out for a logical screen whose short side is this many pixels;
 * the system compositor scales the buffer to the real display. */
#define LOGICAL_MIN_SIDE 720

uint32_t    g_mem_lower_kb = 640;
uint32_t    g_mem_upper_kb = 2048 * 1024;
const char *g_platform_name = "Android";

static struct timespec t0;

static uint32_t now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)((t.tv_sec - t0.tv_sec) * 1000L + (t.tv_nsec - t0.tv_nsec) / 1000000L);
}

uint32_t timer_ticks(void)
{
    return now_ms() / (1000 / TIMER_HZ);
}

void platform_local_time(platform_time_t *t)
{
    time_t raw = time(NULL);
    struct tm tm;
    localtime_r(&raw, &tm);
    t->year = tm.tm_year + 1900;
    t->month = tm.tm_mon + 1;
    t->day = tm.tm_mday;
    t->weekday = tm.tm_wday;
    t->hour = tm.tm_hour;
    t->minute = tm.tm_min;
    t->second = tm.tm_sec;
}

static void read_memory(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    unsigned kb;
    if (!f)
        return;
    if (fscanf(f, "MemTotal: %u kB", &kb) == 1)
        g_mem_upper_kb = kb;
    fclose(f);
}

/* ---------------------------------------------------------------- state */

static struct android_app *g_app;
static bool g_has_window;
static bool g_initialized;
static int g_win_w, g_win_h;        /* real window size in pixels */
static int g_fb_w, g_fb_h;          /* logical framebuffer size */
static uint32_t *g_buf;
static bool g_back_consumed;
static long g_frames;

static void setup_geometry(void)
{
    ANativeWindow *win = g_app->window;
    int w = ANativeWindow_getWidth(win), h = ANativeWindow_getHeight(win);
    int mn = w < h ? w : h;
    if (mn <= 0)
        return;

    g_win_w = w;
    g_win_h = h;
    g_fb_w = (int)((int64_t)w * LOGICAL_MIN_SIDE / mn);
    g_fb_h = (int)((int64_t)h * LOGICAL_MIN_SIDE / mn);

    free(g_buf);
    g_buf = malloc((size_t)g_fb_w * (size_t)g_fb_h * sizeof(uint32_t));
    ANativeWindow_setBuffersGeometry(win, g_fb_w, g_fb_h, WINDOW_FORMAT_RGBA_8888);

    if (!g_initialized) {
        desktop_init(g_fb_w, g_fb_h);
        desktop_set_touch_mode(true);
        g_initialized = true;
    } else {
        desktop_resize(g_fb_w, g_fb_h);
    }
    LOG("window %dx%d, logical %dx%d", w, h, g_fb_w, g_fb_h);
}

static void handle_cmd(struct android_app *app, int32_t cmd)
{
    switch (cmd) {
    case APP_CMD_INIT_WINDOW:
        if (app->window) {
            setup_geometry();
            g_has_window = true;
        }
        break;
    case APP_CMD_TERM_WINDOW:
        g_has_window = false;
        break;
    default:
        break;
    }
}

/* ----------------------------------------------------------------- input */

static int map_key(int32_t code, int32_t meta)
{
    bool shift = (meta & AMETA_SHIFT_ON) != 0;

    if (code >= AKEYCODE_A && code <= AKEYCODE_Z)
        return (shift ? 'A' : 'a') + (code - AKEYCODE_A);
    if (code >= AKEYCODE_0 && code <= AKEYCODE_9)
        return '0' + (code - AKEYCODE_0);

    switch (code) {
    case AKEYCODE_SPACE:        return ' ';
    case AKEYCODE_ENTER:
    case AKEYCODE_NUMPAD_ENTER: return '\n';
    case AKEYCODE_DEL:          return '\b';
    case AKEYCODE_TAB:          return '\t';
    case AKEYCODE_ESCAPE:       return 27;
    case AKEYCODE_DPAD_UP:      return KEY_UP;
    case AKEYCODE_DPAD_DOWN:    return KEY_DOWN;
    case AKEYCODE_DPAD_LEFT:    return KEY_LEFT;
    case AKEYCODE_DPAD_RIGHT:   return KEY_RIGHT;
    case AKEYCODE_COMMA:        return ',';
    case AKEYCODE_PERIOD:       return '.';
    case AKEYCODE_MINUS:        return shift ? '_' : '-';
    case AKEYCODE_EQUALS:       return shift ? '+' : '=';
    case AKEYCODE_SLASH:        return shift ? '?' : '/';
    case AKEYCODE_SEMICOLON:    return shift ? ':' : ';';
    case AKEYCODE_APOSTROPHE:   return shift ? '"' : '\'';
    default:                    return 0;
    }
}

static int32_t handle_input(struct android_app *app, AInputEvent *ev)
{
    (void)app;

    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(ev) & AMOTION_EVENT_ACTION_MASK;
        if (g_win_w <= 0 || g_win_h <= 0)
            return 0;
        int x = (int)(AMotionEvent_getX(ev, 0) * (float)g_fb_w / (float)g_win_w);
        int y = (int)(AMotionEvent_getY(ev, 0) * (float)g_fb_h / (float)g_win_h);

        switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
            desktop_mouse(DESKTOP_MOUSE_DOWN, x, y);
            break;
        case AMOTION_EVENT_ACTION_MOVE:
            desktop_mouse(DESKTOP_MOUSE_MOVE, x, y);
            break;
        case AMOTION_EVENT_ACTION_UP:
        case AMOTION_EVENT_ACTION_CANCEL:
            desktop_mouse(DESKTOP_MOUSE_UP, x, y);
            break;
        default:
            return 0;
        }
        return 1;
    }

    if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_KEY) {
        int32_t code = AKeyEvent_getKeyCode(ev);
        int32_t action = AKeyEvent_getAction(ev);

        if (code == AKEYCODE_BACK) {
            if (action == AKEY_EVENT_ACTION_DOWN) {
                g_back_consumed = desktop_handle_back();
                return g_back_consumed;
            }
            if (g_back_consumed) {
                g_back_consumed = false;
                return 1;
            }
            return 0;
        }

        if (action != AKEY_EVENT_ACTION_DOWN)
            return 0;
        int key = map_key(code, AKeyEvent_getMetaState(ev));
        if (!key)
            return 0;
        desktop_key(key);
        return 1;
    }

    return 0;
}

/* ----------------------------------------------------------------- frame */

static void render_frame(void)
{
    if (!g_has_window || !g_app->window || !g_buf)
        return;

    ANativeWindow *win = g_app->window;
    if (ANativeWindow_getWidth(win) != g_win_w || ANativeWindow_getHeight(win) != g_win_h)
        setup_geometry();

    if (desktop_quit_requested()) {
        ANativeActivity_finish(g_app->activity);
        return;
    }
    if (desktop_reboot_requested())
        desktop_init(g_fb_w, g_fb_h);

    if (!desktop_update(now_ms()))
        return;

    ANativeWindow_Buffer b;
    if (ANativeWindow_lock(win, &b, NULL) < 0)
        return;

    desktop_draw(g_buf, g_fb_w, g_fb_h);

    int w = b.width < g_fb_w ? b.width : g_fb_w;
    int h = b.height < g_fb_h ? b.height : g_fb_h;
    for (int y = 0; y < h; y++) {
        uint32_t *dst = (uint32_t *)b.bits + (size_t)y * (size_t)b.stride;
        const uint32_t *src = g_buf + (size_t)y * (size_t)g_fb_w;
        for (int x = 0; x < w; x++) {
            uint32_t p = src[x];    /* 0x00RRGGBB -> RGBA bytes: R, G, B, A */
            dst[x] = 0xFF000000u | ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF);
        }
    }
    ANativeWindow_unlockAndPost(win);

    g_frames++;
    if (g_frames == 1 || g_frames % 120 == 0)
        LOG("frame %ld", g_frames);
}

void android_main(struct android_app *app)
{
    g_app = app;
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;

    clock_gettime(CLOCK_MONOTONIC, &t0);
    read_memory();
    LOG("Astro OS started");

    for (;;) {
        int events;
        struct android_poll_source *source;
        int timeout = g_has_window ? 16 : -1;

        while (ALooper_pollOnce(timeout, NULL, &events, (void **)&source) >= 0) {
            if (source)
                source->process(app, source);
            if (app->destroyRequested) {
                LOG("Astro OS stopped");
                free(g_buf);
                return;
            }
            timeout = 0;
        }
        render_frame();
    }
}
