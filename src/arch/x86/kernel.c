/* Bare-metal x86 entry point, called from src/boot/boot.S. */
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "pic.h"
#include "bootlog.h"
#include "keyboard.h"
#include "kprintf.h"
#include "launcher.h"
#include "sysinfo.h"
#include "timer.h"
#include "vga.h"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_FLAG_MEM         (1u << 0)

/* Prefix of the Multiboot info structure (only what Astro uses for now). */
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;     /* KiB below 1 MiB */
    uint32_t mem_upper;     /* KiB above 1 MiB */
} multiboot_info_t;

uint32_t    g_mem_lower_kb;
uint32_t    g_mem_upper_kb;
const char *g_platform_name = "x86, 32-bit protected mode";

void kernel_main(uint32_t magic, multiboot_info_t *mbi)
{
    vga_init();
    bootlog_banner();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_LIGHT_RED, VGA_BLACK);
        kprintf("  Error: the kernel must be loaded by a Multiboot bootloader.\n");
        for (;;)
            cpu_halt();
    }
    bootlog_step("Multiboot bootloader");

    if (mbi->flags & MULTIBOOT_FLAG_MEM) {
        g_mem_lower_kb = mbi->mem_lower;
        g_mem_upper_kb = mbi->mem_upper;
    }
    bootlog_step("Memory detected");

    gdt_init();
    bootlog_step("GDT loaded");

    idt_init();
    pic_init();
    bootlog_step("Interrupts (IDT + PIC)");

    timer_init();
    bootlog_step("Timer (PIT, 100 Hz)");

    keyboard_init();
    bootlog_step("Keyboard (PS/2)");

    cpu_sti();
    timer_sleep_ms(800);

    launcher_run();
}
