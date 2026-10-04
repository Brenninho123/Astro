#ifndef ASTRO_X86_PIC_H
#define ASTRO_X86_PIC_H

#include "types.h"

/* Remaps the PICs to vectors 32-47 and masks every IRQ. */
void pic_init(void);
void pic_unmask(uint8_t irq);
void pic_eoi(uint8_t irq);

#endif
