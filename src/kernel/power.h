#ifndef ASTRO_POWER_H
#define ASTRO_POWER_H

/* Implemented per platform:
 *   - bare metal (src/arch/x86/power.c): never returns.
 *   - graphical builds (src/desktop/desktop.c): record the request and return;
 *     the platform loop then restarts or closes the application. */
void power_reboot(void);
void power_shutdown(void);

#endif
