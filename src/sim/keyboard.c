/* Simulated keyboard: Windows console key events. */
#include <windows.h>

#include "keyboard.h"
#include "power.h"
#include "sim.h"

void keyboard_init(void)
{
}

int keyboard_getkey(void)
{
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);

    /* The OS is about to idle: show whatever it drew. */
    sim_present();

    for (;;) {
        INPUT_RECORD rec;
        DWORD count = 0;

        if (!ReadConsoleInputW(in, &rec, 1, &count) || count == 0)
            power_shutdown();       /* input closed: nothing left to simulate */

        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            sim_present();
            continue;
        }
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown)
            continue;

        const KEY_EVENT_RECORD *k = &rec.Event.KeyEvent;

        switch (k->wVirtualKeyCode) {
        case VK_UP:     return KEY_UP;
        case VK_DOWN:   return KEY_DOWN;
        case VK_LEFT:   return KEY_LEFT;
        case VK_RIGHT:  return KEY_RIGHT;
        case VK_RETURN: return '\n';
        case VK_BACK:   return '\b';
        case VK_TAB:    return '\t';
        case VK_ESCAPE: return 27;
        }

        WCHAR ch = k->uChar.UnicodeChar;
        if (ch == 3)                /* Ctrl+C closes the simulator */
            power_shutdown();
        if (ch >= 0x20 && ch < 0x7F)
            return (int)ch;
    }
}
