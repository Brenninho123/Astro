#ifndef ASTRO_TIMER_H
#define ASTRO_TIMER_H

#include "types.h"

#define TIMER_HZ 100

/* Implemented per platform (src/arch/x86/timer.c, src/sim/timer.c). */
void     timer_init(void);
uint32_t timer_ticks(void);
void     timer_sleep_ms(uint32_t ms);

#endif
