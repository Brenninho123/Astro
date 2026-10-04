#ifndef ASTRO_POWER_H
#define ASTRO_POWER_H

/* Implemented per platform (src/arch/x86/power.c, src/sim/power.c). */
void power_reboot(void) __attribute__((noreturn));
void power_shutdown(void) __attribute__((noreturn));

#endif
