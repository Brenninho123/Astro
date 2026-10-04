/* Simulated VGA display: an in-memory text buffer drawn to the Windows console. */
#include <windows.h>

#include "sim.h"
#include "vga.h"

static uint16_t buffer[VGA_WIDTH * VGA_HEIGHT];
static int      cursor_x;
static int      cursor_y;
static bool     cursor_visible = true;

static HANDLE   console_out;
static HANDLE   console_in;
static DWORD    saved_in_mode;
static CONSOLE_CURSOR_INFO saved_cursor;
static WORD     saved_attributes = 7;

/* ---- platform hooks used by src/kernel/vga.c ---- */

volatile uint16_t *vga_hw_buffer(void)
{
    return buffer;
}

void vga_hw_set_cursor(int x, int y)
{
    cursor_x = x;
    cursor_y = y;
}

void vga_hw_cursor_enable(bool enable)
{
    cursor_visible = enable;
}

/* ---- console handling ---- */

/* The attribute byte of the VGA text mode has the same bit layout as the Windows
 * console (bit 0 blue, 1 green, 2 red, 3 intensity), so it is used as is. */
static WCHAR cp437_to_unicode(uint8_t c)
{
    switch (c) {
    case CH_UPPER_HALF: return 0x2580;
    case CH_LOWER_HALF: return 0x2584;
    case CH_FULL_BLOCK: return 0x2588;
    default:            return (c >= 0x20 && c < 0x7F) ? (WCHAR)c : L' ';
    }
}

void sim_console_init(void)
{
    console_out = GetStdHandle(STD_OUTPUT_HANDLE);
    console_in  = GetStdHandle(STD_INPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(console_out, &info))
        saved_attributes = info.wAttributes;
    GetConsoleCursorInfo(console_out, &saved_cursor);
    GetConsoleMode(console_in, &saved_in_mode);

    /* Raw key events: no line editing, no echo, no quick-edit selection. */
    SetConsoleMode(console_in, ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS);
    SetConsoleTitleW(L"Astro OS Simulator");

    /* Try to resize the console to exactly 80x25 (shrink the window first). */
    SMALL_RECT tiny = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(console_out, TRUE, &tiny);
    COORD size = { VGA_WIDTH, VGA_HEIGHT };
    SetConsoleScreenBufferSize(console_out, size);
    SMALL_RECT window = { 0, 0, VGA_WIDTH - 1, VGA_HEIGHT - 1 };
    SetConsoleWindowInfo(console_out, TRUE, &window);

    /* Clear everything, including areas outside the 80x25 region. */
    if (GetConsoleScreenBufferInfo(console_out, &info)) {
        DWORD written;
        DWORD cells = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
        COORD origin = { 0, 0 };
        FillConsoleOutputCharacterW(console_out, L' ', cells, origin, &written);
        FillConsoleOutputAttribute(console_out, 7, cells, origin, &written);
    }
}

void sim_console_shutdown(void)
{
    SetConsoleMode(console_in, saved_in_mode);
    SetConsoleCursorInfo(console_out, &saved_cursor);
    SetConsoleTextAttribute(console_out, saved_attributes);

    COORD below = { 0, VGA_HEIGHT - 1 };
    SetConsoleCursorPosition(console_out, below);
}

void sim_present(void)
{
    CHAR_INFO cells[VGA_WIDTH * VGA_HEIGHT];

    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        cells[i].Char.UnicodeChar = cp437_to_unicode((uint8_t)(buffer[i] & 0xFF));
        cells[i].Attributes = (WORD)(buffer[i] >> 8);
    }

    COORD size = { VGA_WIDTH, VGA_HEIGHT };
    COORD origin = { 0, 0 };
    SMALL_RECT region = { 0, 0, VGA_WIDTH - 1, VGA_HEIGHT - 1 };
    WriteConsoleOutputW(console_out, cells, size, origin, &region);

    COORD cursor = { (SHORT)cursor_x, (SHORT)cursor_y };
    SetConsoleCursorPosition(console_out, cursor);

    CONSOLE_CURSOR_INFO ci = { 20, cursor_visible ? TRUE : FALSE };
    SetConsoleCursorInfo(console_out, &ci);
}
