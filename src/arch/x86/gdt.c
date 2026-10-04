#include "gdt.h"
#include "types.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void gdt_flush(uint32_t gdt_ptr);

static struct gdt_entry gdt[3];
static struct gdt_ptr   gdtp;

static void gdt_set(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

void gdt_init(void)
{
    gdtp.limit = sizeof(gdt) - 1;
    gdtp.base  = (uint32_t)(uintptr_t)&gdt;

    gdt_set(0, 0, 0, 0, 0);                    /* null descriptor */
    gdt_set(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);     /* 4 GiB code, ring 0 (selector 0x08) */
    gdt_set(2, 0, 0xFFFFFFFF, 0x92, 0xCF);     /* 4 GiB data, ring 0 (selector 0x10) */

    gdt_flush((uint32_t)(uintptr_t)&gdtp);
}
