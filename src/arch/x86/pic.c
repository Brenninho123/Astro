#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

void pic_init(void)
{
    outb(PIC1_CMD, 0x11);  io_wait();     /* ICW1: start initialization */
    outb(PIC2_CMD, 0x11);  io_wait();
    outb(PIC1_DATA, 0x20); io_wait();     /* ICW2: vector offsets 32 and 40 */
    outb(PIC2_DATA, 0x28); io_wait();
    outb(PIC1_DATA, 0x04); io_wait();     /* ICW3: slave on IRQ2 */
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();     /* ICW4: 8086 mode */
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, 0xFB);                /* everything masked except the cascade (IRQ2) */
    outb(PIC2_DATA, 0xFF);
}

void pic_unmask(uint8_t irq)
{
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = irq < 8 ? irq : (uint8_t)(irq - 8);
    outb(port, inb(port) & (uint8_t)~(1u << bit));
}

void pic_eoi(uint8_t irq)
{
    if (irq >= 8)
        outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}
