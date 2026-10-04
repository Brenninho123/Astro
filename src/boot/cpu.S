# Astro OS - CPU routines in assembly: GDT, IDT and interrupt stubs

.section .text

# void gdt_flush(uint32_t gdt_ptr)
.global gdt_flush
gdt_flush:
    mov 4(%esp), %eax
    lgdt (%eax)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    ljmp $0x08, $.Lflush
.Lflush:
    ret

# void idt_flush(uint32_t idt_ptr)
.global idt_flush
idt_flush:
    mov 4(%esp), %eax
    lidt (%eax)
    ret

# ---- exception (0-31) and IRQ (32-47) stubs ----
# Stack layout when isr_common starts (top first):
#   int_no, err_code, [eip, cs, eflags pushed by the CPU]

.macro ISR_NOERR n
.global isr\n
isr\n:
    push $0
    push $\n
    jmp isr_common
.endm

.macro ISR_ERR n
.global isr\n
isr\n:
    push $\n
    jmp isr_common
.endm

.macro IRQ n
.global irq\n
irq\n:
    push $0
    push $(32+\n)
    jmp isr_common
.endm

.irp n,0,1,2,3,4,5,6,7,9,15,16,18,19,20,21,22,23,24,25,26,27,28,29,31
    ISR_NOERR \n
.endr

.irp n,8,10,11,12,13,14,17,30
    ISR_ERR \n
.endr

.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    IRQ \n
.endr

isr_common:
    pusha
    push %ds
    push %es
    push %fs
    push %gs
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    push %esp                     # registers_t*
    call isr_handler
    add $4, %esp
    pop %gs
    pop %fs
    pop %es
    pop %ds
    popa
    add $8, %esp                  # drop int_no and err_code
    iret

# Table with the address of each stub, indexed by interrupt number.
.section .rodata
.align 4
.global isr_stub_table
isr_stub_table:
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    .long isr\n
.endr
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
    .long irq\n
.endr
