#ifndef ASTRO_KSTRING_H
#define ASTRO_KSTRING_H

#ifdef ASTRO_HOSTED
/* The simulator runs on top of a normal C runtime. */
#include <string.h>
#else

#include "types.h"

size_t strlen(const char *s);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);

void  *memset(void *dst, int value, size_t n);
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
int    memcmp(const void *a, const void *b, size_t n);

#endif /* ASTRO_HOSTED */

#endif
