#include "kprintf.h"
#include "vga.h"

static void put_number(uint32_t value, unsigned base, bool upper, bool negative,
                       int width, char pad)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char buf[34];
    int len = 0;

    do {
        buf[len++] = digits[value % base];
        value /= base;
    } while (value);

    if (negative)
        buf[len++] = '-';

    /* left padding; with '0' the sign goes before the zeros */
    if (pad == '0' && negative) {
        vga_putc('-');
        len--;
        width--;
    }
    for (int i = len; i < width; i++)
        vga_putc(pad);
    while (len)
        vga_putc(buf[--len]);
}

void kvprintf(const char *fmt, va_list ap)
{
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            vga_putc(*fmt);
            continue;
        }

        fmt++;
        char pad = ' ';
        int width = 0;

        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9')
            width = width * 10 + (*fmt++ - '0');

        switch (*fmt) {
        case 'c':
            vga_putc((char)va_arg(ap, int));
            break;
        case 's': {
            const char *s = va_arg(ap, const char *);
            vga_puts(s ? s : "(null)");
            break;
        }
        case 'd': {
            int v = va_arg(ap, int);
            bool neg = v < 0;
            put_number(neg ? 0u - (uint32_t)v : (uint32_t)v, 10, false, neg, width, pad);
            break;
        }
        case 'u':
            put_number(va_arg(ap, uint32_t), 10, false, false, width, pad);
            break;
        case 'x':
            put_number(va_arg(ap, uint32_t), 16, false, false, width, pad);
            break;
        case 'X':
            put_number(va_arg(ap, uint32_t), 16, true, false, width, pad);
            break;
        case 'p':
            vga_puts("0x");
            put_number((uint32_t)(uintptr_t)va_arg(ap, void *), 16, false, false, 8, '0');
            break;
        case '%':
            vga_putc('%');
            break;
        case '\0':
            return;
        default:
            vga_putc('%');
            vga_putc(*fmt);
            break;
        }
    }
}

void kprintf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}
