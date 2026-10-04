/* Simulated power control: "restart" re-runs the simulated boot, "shut down" exits the program. */
#include <windows.h>
#include <stdlib.h>

#include "kprintf.h"
#include "power.h"
#include "sim.h"
#include "vga.h"

void power_reboot(void)
{
    sim_reboot();
}

void power_shutdown(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_cursor_enable(false);
    kprintf("\n  Astro has shut down. You can close this window.\n");
    sim_present();
    Sleep(900);

    sim_console_shutdown();
    exit(0);
}
