#ifndef ASTRO_KPRINTF_H
#define ASTRO_KPRINTF_H

#include "types.h"

/* Minimal printf: %c %s %d %u %x %X %p %% with width and zero padding (e.g. %02d). */
void kprintf(const char *fmt, ...);
void kvprintf(const char *fmt, va_list ap);

#endif
