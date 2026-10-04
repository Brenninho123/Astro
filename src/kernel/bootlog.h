#ifndef ASTRO_BOOTLOG_H
#define ASTRO_BOOTLOG_H

/* Boot screen helpers shared by every platform entry point. */
void bootlog_banner(void);
void bootlog_step(const char *what);

#endif
