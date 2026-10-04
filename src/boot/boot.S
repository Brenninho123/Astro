# Astro OS - entry point (Multiboot 1)
# Any compliant bootloader (GRUB, QEMU -kernel) loads this kernel in
# 32-bit protected mode and jumps to _start.

.set ALIGN,    1<<0               # align modules on page boundaries
.set MEMINFO,  1<<1               # request the memory map
.set FLAGS,    ALIGN | MEMINFO
.set MAGIC,    0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.section .bss
.align 16
stack_bottom:
.skip 16384                       # 16 KiB stack
stack_top:

.section .text
.global _start
.type _start, @function
_start:
    cli
    mov $stack_top, %esp
    push %ebx                     # 2nd argument: pointer to multiboot_info
    push %eax                     # 1st argument: bootloader magic
    call kernel_main
.Lhang:
    cli
    hlt
    jmp .Lhang
.size _start, . - _start
