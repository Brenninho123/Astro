#include "idt.h"
#include "io.h"
#include "kprintf.h"
#include "pic.h"
#include "vga.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void idt_flush(uint32_t idt_ptr);
extern uint32_t isr_stub_table[48];

static struct idt_entry idt[256];
static struct idt_ptr   idtp;
static isr_handler_t    handlers[256];

static const char *const exception_names[32] = {
    "Divide by zero", "Debug", "NMI", "Breakpoint", "Overflow",
    "Bound range exceeded", "Invalid opcode", "Device not available",
    "Double fault", "Coprocessor segment overrun", "Invalid TSS",
    "Segment not present", "Stack fault", "General protection fault",
    "Page fault", "Reserved", "x87 floating point", "Alignment check",
    "Machine check", "SIMD floating point", "Virtualization", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Security", "Reserved",
};

static void idt_set_gate(uint8_t n, uint32_t base)
{
    idt[n].base_low  = base & 0xFFFF;
    idt[n].base_high = (base >> 16) & 0xFFFF;
    idt[n].selector  = 0x08;     /* kernel code segment */
    idt[n].zero      = 0;
    idt[n].flags     = 0x8E;     /* present, ring 0, 32-bit interrupt gate */
}

void idt_init(void)
{
    for (int i = 0; i < 48; i++)
        idt_set_gate((uint8_t)i, isr_stub_table[i]);

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)(uintptr_t)&idt;
    idt_flush((uint32_t)(uintptr_t)&idtp);
}

void isr_register(uint8_t int_no, isr_handler_t handler)
{
    handlers[int_no] = handler;
}

static void kernel_panic(registers_t *r)
{
    cpu_cli();
    vga_cursor_enable(false);
    vga_set_color(VGA_WHITE, VGA_RED);
    vga_clear();
    kprintf("\n  *** ASTRO KERNEL PANIC ***\n\n");
    kprintf("  Exception %u: %s\n", r->int_no, exception_names[r->int_no]);
    kprintf("  Error code: %08x\n\n", r->err_code);
    kprintf("  EIP=%08x  CS=%04x  EFLAGS=%08x\n", r->eip, r->cs, r->eflags);
    kprintf("  EAX=%08x  EBX=%08x  ECX=%08x  EDX=%08x\n", r->eax, r->ebx, r->ecx, r->edx);
    kprintf("  ESI=%08x  EDI=%08x  EBP=%08x  ESP=%08x\n", r->esi, r->edi, r->ebp, r->esp);
    kprintf("\n  System halted.\n");
    for (;;)
        cpu_halt();
}

/* Called by isr_common (src/boot/cpu.S). */
void isr_handler(registers_t *regs)
{
    if (handlers[regs->int_no])
        handlers[regs->int_no](regs);
    else if (regs->int_no < IRQ_BASE)
        kernel_panic(regs);

    if (regs->int_no >= IRQ_BASE && regs->int_no < IRQ_BASE + 16)
        pic_eoi((uint8_t)(regs->int_no - IRQ_BASE));
}
