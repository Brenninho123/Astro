#include "power.h"
#include "io.h"
#include "kprintf.h"
#include "vga.h"

void power_reboot(void)
{
    cpu_cli();

    /* Ask the keyboard controller (8042) for a CPU reset. */
    while (inb(0x64) & 0x02)
        ;
    outb(0x64, 0xFE);

    for (;;)
        cpu_halt();
}

void power_shutdown(void)
{
    cpu_cli();

    /* ACPI shutdown ports used by emulators (there is no real ACPI driver yet). */
    outw(0x604, 0x2000);        /* QEMU >= 2.0 */
    outw(0xB004, 0x2000);       /* Bochs / old QEMU */
    outw(0x4004, 0x3400);       /* VirtualBox */

    vga_cursor_enable(false);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    kprintf("\n  Astro has shut down. It is now safe to turn off the computer.\n");
    for (;;)
        cpu_halt();
}
