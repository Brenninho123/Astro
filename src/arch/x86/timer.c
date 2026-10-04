#include "timer.h"
#include "idt.h"
#include "io.h"
#include "pic.h"

#define PIT_FREQ 1193182u

static volatile uint32_t ticks;

static void timer_irq(registers_t *regs)
{
    (void)regs;
    ticks++;
}

void timer_init(void)
{
    uint32_t divisor = PIT_FREQ / TIMER_HZ;

    outb(0x43, 0x36);                       /* channel 0, lobyte/hibyte, square wave */
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)(divisor >> 8));

    irq_register(0, timer_irq);
    pic_unmask(0);
}

uint32_t timer_ticks(void)
{
    return ticks;
}

void timer_sleep_ms(uint32_t ms)
{
    uint32_t end = ticks + (ms * TIMER_HZ + 999) / 1000;
    while ((int32_t)(end - ticks) > 0)
        cpu_halt();                         /* wakes up on the next interrupt */
}
