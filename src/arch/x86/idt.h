#ifndef ASTRO_X86_IDT_H
#define ASTRO_X86_IDT_H

#include "types.h"

/* State saved by the stubs in src/boot/cpu.S (field order = push order). */
typedef struct registers {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   /* pusha */
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;                          /* pushed by the CPU */
} registers_t;

typedef void (*isr_handler_t)(registers_t *regs);

#define IRQ_BASE 32

void idt_init(void);
void isr_register(uint8_t int_no, isr_handler_t handler);

static inline void irq_register(uint8_t irq, isr_handler_t handler)
{
    isr_register((uint8_t)(IRQ_BASE + irq), handler);
}

#endif
