#ifndef ASTRO_X86_GDT_H
#define ASTRO_X86_GDT_H

/* Loads Astro's own GDT (flat model: null, ring 0 code, ring 0 data). */
void gdt_init(void);

#endif
