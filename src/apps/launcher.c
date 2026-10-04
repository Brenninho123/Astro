#include "launcher.h"
#include "launcher_icon.h"
#include "keyboard.h"
#include "kprintf.h"
#include "power.h"
#include "shell.h"
#include "sysinfo.h"
#include "timer.h"
#include "version.h"
#include "vga.h"

#define COL_BG       vga_attr(VGA_LIGHT_GREY, VGA_BLACK)
#define COL_BAR      vga_attr(VGA_BLACK, VGA_LIGHT_GREY)
#define COL_TITLE    vga_attr(VGA_LIGHT_MAGENTA, VGA_BLACK)
#define COL_SUBTITLE vga_attr(VGA_LIGHT_CYAN, VGA_BLACK)
#define COL_SELECTED vga_attr(VGA_BLACK, VGA_LIGHT_MAGENTA)
#define COL_DESC     vga_attr(VGA_DARK_GREY, VGA_BLACK)

#define MENU_X 4
#define MENU_Y 12
#define MENU_W 72

typedef struct {
    const char *name;
    const char *desc;
    void (*run)(void);
} app_t;

static void wait_any_key(void)
{
    vga_set_color(VGA_DARK_GREY, VGA_BLACK);
    kprintf("\n  Press any key to go back...");
    keyboard_getkey();
}

static void app_terminal(void)
{
    shell_run();
}

static void app_sysinfo(void)
{
    uint32_t s = timer_ticks() / TIMER_HZ;

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_cursor_enable(false);
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("\n  System information\n\n");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("  System:         %s OS %s\n", ASTRO_NAME, ASTRO_VERSION);
    kprintf("  Platform:       %s\n", g_platform_name);
    kprintf("  Lower memory:   %u KiB\n", g_mem_lower_kb);
    kprintf("  Upper memory:   %u MiB\n", g_mem_upper_kb / 1024);
    kprintf("  Uptime:         %02u:%02u:%02u\n", s / 3600, (s / 60) % 60, s % 60);
    wait_any_key();
}

static void app_about(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_cursor_enable(false);
    launcher_icon_draw(2, 2);

    vga_puts_at(24, 4, "ASTRO OS", COL_TITLE);
    vga_puts_at(24, 6, "An operating system written in C.", COL_BG);
    vga_puts_at(24, 7, "Version " ASTRO_VERSION, COL_BG);
    vga_puts_at(24, 9, "Monolithic kernel with interrupts, timer, keyboard,", COL_DESC);
    vga_puts_at(24, 10, "a text-mode display, a shell and this launcher.", COL_DESC);

    vga_set_cursor(0, 14);
    wait_any_key();
}

static void app_reboot(void)   { power_reboot(); }
static void app_shutdown(void) { power_shutdown(); }

static const app_t apps[] = {
    { "Terminal",    "Astro command shell",         app_terminal },
    { "System Info", "Memory, uptime and version",  app_sysinfo },
    { "About",       "About Astro OS",              app_about },
    { "Restart",     "Restart the computer",        app_reboot },
    { "Shut down",   "Power off the computer",      app_shutdown },
};

#define NUM_APPS ((int)(sizeof(apps) / sizeof(apps[0])))

static void draw_static(void)
{
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_cursor_enable(false);

    /* top bar */
    vga_fill(0, 0, VGA_WIDTH, 1, ' ', COL_BAR);
    vga_puts_at(2, 0, "Astro Launcher", COL_BAR);
    vga_puts_at(VGA_WIDTH - 2 - 6, 0, "v" ASTRO_VERSION, COL_BAR);

    /* header with the icon */
    launcher_icon_draw(4, 2);
    vga_puts_at(24, 4, "A S T R O", COL_TITLE);
    vga_puts_at(24, 6, "Choose an application to open", COL_SUBTITLE);

    /* footer */
    vga_fill(0, VGA_HEIGHT - 1, VGA_WIDTH, 1, ' ', COL_BAR);
    vga_puts_at(2, VGA_HEIGHT - 1, "Arrows: navigate    Enter: open", COL_BAR);
}

static void draw_menu(int selected)
{
    for (int i = 0; i < NUM_APPS; i++) {
        int y = MENU_Y + i * 2;
        bool sel = i == selected;
        uint8_t attr = sel ? COL_SELECTED : COL_BG;

        vga_fill(MENU_X, y, MENU_W, 1, ' ', attr);
        vga_puts_at(MENU_X + 2, y, apps[i].name, attr);
        vga_puts_at(MENU_X + 20, y, apps[i].desc, sel ? COL_SELECTED : COL_DESC);
    }
}

void launcher_run(void)
{
    int selected = 0;

    for (;;) {
        draw_static();
        draw_menu(selected);

        for (;;) {
            int key = keyboard_getkey();

            if (key == KEY_UP || key == KEY_LEFT) {
                selected = (selected + NUM_APPS - 1) % NUM_APPS;
                draw_menu(selected);
            } else if (key == KEY_DOWN || key == KEY_RIGHT) {
                selected = (selected + 1) % NUM_APPS;
                draw_menu(selected);
            } else if (key == '\n') {
                apps[selected].run();
                break;      /* the app finished: redraw the launcher */
            }
        }
    }
}
