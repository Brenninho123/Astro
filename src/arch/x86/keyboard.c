#include "keyboard.h"
#include "idt.h"
#include "io.h"
#include "pic.h"

#define KBD_DATA 0x60

#define SC_LSHIFT 0x2A
#define SC_RSHIFT 0x36
#define SC_CAPS   0x3A

/* Scancode set 1, US layout. */
static const char map_normal[58] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ',
};

static const char map_shift[58] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ',
};

#define BUF_SIZE 64     /* power of two */

static volatile uint16_t buffer[BUF_SIZE];
static volatile uint32_t head;      /* written by the IRQ handler */
static volatile uint32_t tail;      /* written by the consumer */

static bool shift;
static bool caps;
static bool extended;

static void push(uint16_t key)
{
    uint32_t next = (head + 1) & (BUF_SIZE - 1);
    if (next == tail)
        return;                     /* buffer full: drop the key */
    buffer[head] = key;
    head = next;
}

static void keyboard_irq(registers_t *regs)
{
    (void)regs;
    uint8_t sc = inb(KBD_DATA);

    if (sc == 0xE0) {
        extended = true;
        return;
    }

    bool released = sc & 0x80;
    uint8_t code = sc & 0x7F;

    if (extended) {
        extended = false;
        if (released)
            return;
        switch (code) {
        case 0x48: push(KEY_UP);    break;
        case 0x50: push(KEY_DOWN);  break;
        case 0x4B: push(KEY_LEFT);  break;
        case 0x4D: push(KEY_RIGHT); break;
        }
        return;
    }

    if (code == SC_LSHIFT || code == SC_RSHIFT) {
        shift = !released;
        return;
    }
    if (released)
        return;
    if (code == SC_CAPS) {
        caps = !caps;
        return;
    }
    if (code >= sizeof(map_normal))
        return;

    char c = shift ? map_shift[code] : map_normal[code];
    if (caps && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        c ^= 0x20;                  /* swap upper/lower case */
    if (c)
        push((uint8_t)c);
}

void keyboard_init(void)
{
    irq_register(1, keyboard_irq);
    pic_unmask(1);
}

int keyboard_getkey(void)
{
    while (head == tail)
        cpu_halt();                 /* sleep until the next interrupt */

    uint16_t key = buffer[tail];
    tail = (tail + 1) & (BUF_SIZE - 1);
    return key;
}
