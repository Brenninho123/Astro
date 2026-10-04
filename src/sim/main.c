/* Entry point of the Astro OS Simulator for Windows. */
#include <setjmp.h>
#include <stdint.h>

#include "bootlog.h"
#include "keyboard.h"
#include "launcher.h"
#include "sim.h"
#include "sysinfo.h"
#include "timer.h"
#include "vga.h"

uint32_t    g_mem_lower_kb = 640;
uint32_t    g_mem_upper_kb = 128 * 1024;    /* simulated: 128 MiB */
const char *g_platform_name = "Windows simulator";

static jmp_buf reboot_env;

void sim_reboot(void)
{
    longjmp(reboot_env, 1);
}

/* Prints a boot step and pauses briefly so the boot sequence is visible. */
static void step(const char *what)
{
    bootlog_step(what);
    timer_sleep_ms(180);
}

int main(void)
{
    sim_console_init();

    setjmp(reboot_env);         /* a simulated reboot jumps back here */

    timer_init();
    vga_init();
    bootlog_banner();

    step("Simulated hardware layer (Windows console)");
    step("Memory detected");
    step("Display (simulated VGA, 80x25)");
    step("Timer (simulated, 100 Hz)");
    keyboard_init();
    step("Keyboard (Windows console)");
    timer_sleep_ms(500);

    launcher_run();
}
