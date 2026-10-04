#ifndef ASTRO_PLATFORM_H
#define ASTRO_PLATFORM_H

/* Services every graphical host (Windows simulator, Android app) must provide
 * besides the framebuffer and input events handled through desktop.h. */

typedef struct {
    int year, month, day;       /* month: 1-12 */
    int weekday;                /* 0 = Sunday */
    int hour, minute, second;
} platform_time_t;

void platform_local_time(platform_time_t *t);

#endif
