#ifndef ASTRO_SHELL_H
#define ASTRO_SHELL_H

#include "types.h"

/* Event-driven terminal: output goes through the VGA text layer (vga.h), input is
 * pushed one key at a time, so it works both in a blocking loop (bare metal) and
 * inside a graphical window. */

/* Clears the screen, prints the banner and the first prompt. */
void shell_start(void);

/* Feeds one key (ASCII; KEY_* values are ignored). Returns true when the user
 * typed "exit" and the shell should be closed. */
bool shell_key(int key);

#endif
