#include "serial.h"
#include "hal.h"

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x80);
    outb(COM1_PORT + 0, 0x03);
    outb(COM1_PORT + 1, 0x00);
    outb(COM1_PORT + 3, 0x03);
    outb(COM1_PORT + 2, 0xC7);
    outb(COM1_PORT + 4, 0x0B);
}

void serial_putc(char c) {
    while (!(inb(COM1_PORT + 5) & 0x20));
    outb(COM1_PORT, c);
}

void serial_puts(const char* s) {
    while (*s) serial_putc(*s++);
}

static void serial_puthex(uint64_t val, int digits) {
    for (int i = digits - 1; i >= 0; i--) {
        uint8_t nibble = (val >> (i * 4)) & 0xF;
        serial_putc(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}

void serial_hex64(uint64_t val) {
    serial_puts("0x");
    serial_puthex(val, 16);
}

void serial_hex32(uint32_t val) {
    serial_puts("0x");
    serial_puthex(val, 8);
}

void serial_hex16(uint16_t val) {
    serial_puts("0x");
    serial_puthex(val, 4);
}

void serial_hex8(uint8_t val) {
    serial_puts("0x");
    serial_puthex(val, 2);
}

static void serial_print_dec(uint64_t val) {
    char buf[21];
    int i = 20;
    buf[i] = '\0';
    if (val == 0) {
        serial_putc('0');
        return;
    }
    while (val > 0 && i > 0) {
        buf[--i] = '0' + (val % 10);
        val /= 10;
    }
    serial_puts(&buf[i]);
}

void serial_printf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (const char* p = fmt; *p; p++) {
        if (*p != '%') {
            serial_putc(*p);
            continue;
        }
        p++;
        int lng = 0;
        while (*p == 'l') {
            lng++;
            p++;
        }

        switch (*p) {
            case 's': {
                const char* s = va_arg(args, const char*);
                serial_puts(s ? s : "(null)");
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                serial_putc(c);
                break;
            }
            case 'd':
            case 'i': {
                int val = va_arg(args, int);
                if (val < 0) {
                    serial_putc('-');
                    val = -val;
                }
                serial_print_dec((uint64_t)val);
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                serial_print_dec(val);
                break;
            }
            case 'x':
            case 'X': {
                if (lng >= 1) {
                    uint64_t val = va_arg(args, uint64_t);
                    serial_puts("0x");
                    serial_puthex(val, 16);
                } else {
                    uint32_t val = va_arg(args, uint32_t);
                    serial_puts("0x");
                    serial_puthex(val, 8);
                }
                break;
            }
            case '%':
                serial_putc('%');
                break;
            default:
                serial_putc('%');
                serial_putc(*p);
                break;
        }
    }

    va_end(args);
}
