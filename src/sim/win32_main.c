/*
 * Astro OS Simulator for Windows.
 *
 * Runs the graphical home screen in a normal Win32 window. It is just a program:
 * it never touches the real hardware and does not replace Windows.
 */
#include <windows.h>

#include <stdint.h>
#include <stdlib.h>

#include "desktop.h"
#include "keyboard.h"
#include "platform.h"
#include "sysinfo.h"
#include "timer.h"

#define START_W 1024
#define START_H 640

uint32_t    g_mem_lower_kb = 640;
uint32_t    g_mem_upper_kb = 128 * 1024;    /* simulated: 128 MiB */
const char *g_platform_name = "Windows simulator";

static ULONGLONG t0;

static uint32_t now_ms(void)
{
    return (uint32_t)(GetTickCount64() - t0);
}

uint32_t timer_ticks(void)
{
    return now_ms() / (1000 / TIMER_HZ);
}

void platform_local_time(platform_time_t *t)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    t->year = st.wYear;
    t->month = st.wMonth;
    t->day = st.wDay;
    t->weekday = st.wDayOfWeek;
    t->hour = st.wHour;
    t->minute = st.wMinute;
    t->second = st.wSecond;
}

/* ---------------------------------------------------------------- window */

static uint32_t *pixels;
static int fb_w, fb_h;
static BITMAPINFO bmi;

static void resize_buffer(int w, int h)
{
    if (w <= 0 || h <= 0)
        return;
    free(pixels);
    pixels = malloc((size_t)w * (size_t)h * sizeof(uint32_t));
    fb_w = w;
    fb_h = h;

    ZeroMemory(&bmi, sizeof bmi);
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;            /* top-down */
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE:
        resize_buffer(LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        if (pixels) {
            desktop_draw(pixels, fb_w, fb_h);
            SetDIBitsToDevice(dc, 0, 0, (DWORD)fb_w, (DWORD)fb_h, 0, 0, 0, (UINT)fb_h,
                              pixels, &bmi, DIB_RGB_COLORS);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
        desktop_mouse(DESKTOP_MOUSE_MOVE, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        desktop_mouse(DESKTOP_MOUSE_DOWN, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        desktop_mouse(DESKTOP_MOUSE_UP, (short)LOWORD(lp), (short)HIWORD(lp));
        return 0;

    case WM_KEYDOWN:
        switch (wp) {
        case VK_UP:    desktop_key(KEY_UP);    return 0;
        case VK_DOWN:  desktop_key(KEY_DOWN);  return 0;
        case VK_LEFT:  desktop_key(KEY_LEFT);  return 0;
        case VK_RIGHT: desktop_key(KEY_RIGHT); return 0;
        }
        break;

    case WM_CHAR: {
        int c = (int)wp;
        if (c == '\r')
            c = '\n';
        if (c == '\n' || c == '\b' || c == '\t' || c == 27 || (c >= 0x20 && c < 0x7F))
            desktop_key(c);
        return 0;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    (void)prev;
    (void)cmdline;

    SetProcessDPIAware();
    t0 = GetTickCount64();

    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(inst, MAKEINTRESOURCE(1));
    wc.lpszClassName = "AstroSimWindow";
    RegisterClassA(&wc);

    RECT r = { 0, 0, START_W, START_H };
    AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowA("AstroSimWindow", "Astro OS", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top,
                              NULL, NULL, inst, NULL);
    if (!hwnd)
        return 1;
    ShowWindow(hwnd, show);

    RECT client;
    GetClientRect(hwnd, &client);
    resize_buffer(client.right, client.bottom);
    desktop_init(fb_w, fb_h);

    for (;;) {
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT)
                return 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (desktop_quit_requested()) {
            DestroyWindow(hwnd);
            continue;
        }
        if (desktop_reboot_requested())
            desktop_init(fb_w, fb_h);

        if (desktop_update(now_ms()))
            InvalidateRect(hwnd, NULL, FALSE);

        MsgWaitForMultipleObjects(0, NULL, FALSE, 16, QS_ALLINPUT);
    }
}
