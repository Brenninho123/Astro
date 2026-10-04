#include "shell.h"
#include "keyboard.h"
#include "kprintf.h"
#include "kstring.h"
#include "power.h"
#include "sysinfo.h"
#include "timer.h"
#include "version.h"
#include "vga.h"

#define SHELL_LINE_MAX 128

typedef struct {
    const char *name;
    const char *help;
    bool (*run)(const char *args);      /* returns true to leave the shell */
} command_t;

static bool cmd_help(const char *args);

static bool cmd_clear(const char *args)
{
    (void)args;
    vga_clear();
    return false;
}

static bool cmd_echo(const char *args)
{
    kprintf("%s\n", args);
    return false;
}

static bool cmd_ver(const char *args)
{
    (void)args;
    kprintf("%s OS version %s (%s)\n", ASTRO_NAME, ASTRO_VERSION, g_platform_name);
    return false;
}

static bool cmd_uptime(const char *args)
{
    (void)args;
    uint32_t s = timer_ticks() / TIMER_HZ;
    kprintf("Up for %02u:%02u:%02u\n", s / 3600, (s / 60) % 60, s % 60);
    return false;
}

static bool cmd_mem(const char *args)
{
    (void)args;
    kprintf("Lower memory:  %u KiB\n", g_mem_lower_kb);
    kprintf("Upper memory:  %u KiB (%u MiB)\n", g_mem_upper_kb, g_mem_upper_kb / 1024);
    return false;
}

static bool cmd_reboot(const char *args)
{
    (void)args;
    power_reboot();
}

static bool cmd_shutdown(const char *args)
{
    (void)args;
    power_shutdown();
}

static bool cmd_exit(const char *args)
{
    (void)args;
    return true;
}

static const command_t commands[] = {
    { "help",     "list the available commands", cmd_help },
    { "clear",    "clear the screen",            cmd_clear },
    { "echo",     "print the given text",        cmd_echo },
    { "ver",      "show the Astro version",      cmd_ver },
    { "uptime",   "time since boot",             cmd_uptime },
    { "mem",      "detected memory",             cmd_mem },
    { "reboot",   "restart the computer",        cmd_reboot },
    { "shutdown", "power off the computer",      cmd_shutdown },
    { "exit",     "go back to the launcher",     cmd_exit },
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

static bool cmd_help(const char *args)
{
    (void)args;
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        kprintf("  %s", commands[i].name);
        for (size_t pad = strlen(commands[i].name); pad < 10; pad++)
            vga_putc(' ');
        kprintf("%s\n", commands[i].help);
    }
    return false;
}

static void prompt(void)
{
    vga_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("astro");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("> ");
}

/* Returns true if the shell should exit. */
static bool execute(char *line)
{
    while (*line == ' ')
        line++;
    if (!*line)
        return false;

    char *args = line;
    while (*args && *args != ' ')
        args++;
    if (*args) {
        *args++ = '\0';
        while (*args == ' ')
            args++;
    }

    for (size_t i = 0; i < NUM_COMMANDS; i++)
        if (strcmp(line, commands[i].name) == 0)
            return commands[i].run(args);

    kprintf("unknown command: %s (type 'help')\n", line);
    return false;
}

void shell_run(void)
{
    char line[SHELL_LINE_MAX];

    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
    vga_cursor_enable(true);
    kprintf("%s OS %s terminal - type 'help' to list the commands.\n\n",
            ASTRO_NAME, ASTRO_VERSION);

    for (;;) {
        size_t len = 0;
        prompt();

        for (;;) {
            int key = keyboard_getkey();

            if (key == '\n') {
                vga_putc('\n');
                break;
            }
            if (key == '\b') {
                if (len > 0) {
                    len--;
                    vga_putc('\b');
                }
            } else if (key >= ' ' && key < 0x7F && len < SHELL_LINE_MAX - 1) {
                line[len++] = (char)key;
                vga_putc((char)key);
            }
        }

        line[len] = '\0';
        if (execute(line))
            return;
    }
}
