#ifndef ASTRO_KEYBOARD_H
#define ASTRO_KEYBOARD_H

#include "types.h"

/* Special keys returned by keyboard_getkey() (values above ASCII). */
#define KEY_UP    0x100
#define KEY_DOWN  0x101
#define KEY_LEFT  0x102
#define KEY_RIGHT 0x103

/* Implemented per platform (src/arch/x86/keyboard.c, src/sim/keyboard.c). */
void keyboard_init(void);

/* Blocks until a key is pressed. Returns an ASCII character or a KEY_* value. */
int keyboard_getkey(void);

#endif
