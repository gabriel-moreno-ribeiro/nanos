/* VGA text mode, the COM1 serial port, and a small printf on top of both. */
#include "kernel.h"

/* ---------------------------------------------------------------- VGA ---- */
#define VGA_MEM ((volatile uint16_t *)0xB8000)
#define VGA_COLS 80
#define VGA_ROWS 25
static int vga_row, vga_col;
static const uint8_t vga_attr = 0x0F; /* white on black */

static void vga_cursor(void) {
    uint16_t pos = (uint16_t)(vga_row * VGA_COLS + vga_col);
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
    outb(0x3D4, 0x0E);
    outb(0x3D5, pos >> 8);
}

void vga_clear(void) {
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++) VGA_MEM[i] = (uint16_t)(vga_attr << 8) | ' ';
    vga_row = vga_col = 0;
    vga_cursor();
}

void vga_init(void) { vga_clear(); }

static void vga_scroll(void) {
    for (int i = 0; i < (VGA_ROWS - 1) * VGA_COLS; i++) VGA_MEM[i] = VGA_MEM[i + VGA_COLS];
    for (int i = (VGA_ROWS - 1) * VGA_COLS; i < VGA_ROWS * VGA_COLS; i++) VGA_MEM[i] = (uint16_t)(vga_attr << 8) | ' ';
    vga_row = VGA_ROWS - 1;
}

void vga_putc(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            VGA_MEM[vga_row * VGA_COLS + vga_col] = (uint16_t)(vga_attr << 8) | ' ';
        }
    } else {
        VGA_MEM[vga_row * VGA_COLS + vga_col] = (uint16_t)(vga_attr << 8) | (uint8_t)c;
        if (++vga_col == VGA_COLS) {
            vga_col = 0;
            vga_row++;
        }
    }
    if (vga_row == VGA_ROWS) vga_scroll();
    vga_cursor();
}

/* ------------------------------------------------------------- serial ---- */
#define COM1 0x3F8

void serial_init(void) {
    outb(COM1 + 1, 0x00); /* no interrupts */
    outb(COM1 + 3, 0x80); /* enable divisor latch */
    outb(COM1 + 0, 0x01); /* 115200 baud */
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03); /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7); /* FIFO enabled and cleared */
    outb(COM1 + 4, 0x0B); /* DTR, RTS, OUT2 */
}

void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) {}
    outb(COM1, (uint8_t)c);
}

int serial_getc(void) {
    if (inb(COM1 + 5) & 0x01) return inb(COM1);
    return -1;
}

/* ------------------------------------------------------------- printf ---- */
void putc(char c) {
    if (c == '\n') serial_putc('\r');
    serial_putc(c);
    vga_putc(c);
}

void puts(const char *s) {
    while (*s) putc(*s++);
}

static int left_justify;

static void put_padded(const char *s, int width, char pad) {
    int len = (int)strlen(s);
    if (left_justify) {
        puts(s);
        while (width-- > len) putc(' ');
        return;
    }
    while (width-- > len) putc(pad);
    puts(s);
}

/* Supports %d %u %x %s %c %% with optional '-' (left justify), zero padding and width (e.g. %08x, %-5d). */
void kvprintf(const char *fmt, va_list ap) {
    char buf[34];
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            putc(*fmt);
            continue;
        }
        fmt++;
        char pad = ' ';
        int width = 0;
        left_justify = 0;
        if (*fmt == '-') {
            left_justify = 1;
            fmt++;
        }
        if (*fmt == '0') {
            pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') width = width * 10 + (*fmt++ - '0');
        switch (*fmt) {
            case 'd': {
                int v = va_arg(ap, int);
                if (v < 0) {
                    buf[0] = '-';
                    utoa((uint32_t)(-v), buf + 1, 10);
                } else {
                    utoa((uint32_t)v, buf, 10);
                }
                put_padded(buf, width, pad);
                break;
            }
            case 'u': put_padded(utoa(va_arg(ap, uint32_t), buf, 10), width, pad); break;
            case 'x': put_padded(utoa(va_arg(ap, uint32_t), buf, 16), width, pad); break;
            case 's': {
                const char *s = va_arg(ap, const char *);
                put_padded(s ? s : "(null)", width, ' ');
                break;
            }
            case 'c': putc((char)va_arg(ap, int)); break;
            case '%': putc('%'); break;
            default: putc('%'); putc(*fmt); break;
        }
    }
}

void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void panic(const char *msg) {
    cli();
    kprintf("\nPANIC: %s\n", msg);
    for (;;) hlt();
}

/* QEMU's isa-debug-exit device: the exit status becomes (code << 1) | 1. */
void qemu_exit(uint32_t code) {
    outl(0xF4, code);
    for (;;) hlt();
}
