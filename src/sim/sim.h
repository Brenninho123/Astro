#ifndef ASTRO_SIM_H
#define ASTRO_SIM_H

/*
 * Astro OS Simulator for Windows.
 *
 * The simulator compiles the same launcher, shell and text-mode logic as the
 * bare-metal kernel, but replaces the hardware layer with Win32 console calls.
 * It runs as a normal program inside Windows and never touches the real hardware.
 */

void sim_console_init(void);
void sim_console_shutdown(void);

/* Draws the simulated VGA screen to the Windows console. */
void sim_present(void);

/* Restarts the simulated machine (jumps back to the simulated boot). */
void sim_reboot(void) __attribute__((noreturn));

#endif
