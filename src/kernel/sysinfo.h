#ifndef ASTRO_SYSINFO_H
#define ASTRO_SYSINFO_H

#include "types.h"

/* Defined by each platform entry point (bare-metal kernel or Windows simulator). */
extern uint32_t    g_mem_lower_kb;
extern uint32_t    g_mem_upper_kb;
extern const char *g_platform_name;

#endif
