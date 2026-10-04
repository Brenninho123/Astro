#ifndef ASTRO_DESKTOP_H
#define ASTRO_DESKTOP_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Astro's graphical home screen: boot splash, wallpaper, clock, app grid, dock,
 * the Astro Launcher overlay (with search) and the apps themselves.
 *
 * It is platform independent. A host (Win32 window, Android activity) feeds it
 * input events and a pixel buffer:
 *
 *     desktop_init(w, h);
 *     loop {
 *         desktop_mouse() / desktop_key() ...      // events
 *         if (desktop_update(now_ms))             // advance animations
 *             desktop_draw(pixels, w, h);         // render a full frame
 *         if (desktop_reboot_requested()) desktop_init(w, h);
 *         if (desktop_quit_requested()) break;
 *     }
 */

#define DESKTOP_MOUSE_MOVE 0
#define DESKTOP_MOUSE_DOWN 1
#define DESKTOP_MOUSE_UP   2

void desktop_init(int w, int h);                 /* (re)boots: shows the splash */
void desktop_resize(int w, int h);
void desktop_set_touch_mode(bool touch);         /* on-screen keyboard, no hover */

void desktop_mouse(int kind, int x, int y);     /* touch is mapped to the mouse */
void desktop_key(int key);                       /* ASCII or KEY_* from keyboard.h */
bool desktop_handle_back(void);                  /* Android back; true if consumed */

bool desktop_update(uint32_t now_ms);            /* true when a redraw is needed */
void desktop_draw(uint32_t *pixels, int w, int h);

bool desktop_quit_requested(void);
bool desktop_reboot_requested(void);             /* clears the request */

#endif
