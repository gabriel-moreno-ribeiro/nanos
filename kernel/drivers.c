/* Programmable interval timer and PS/2 keyboard. */
#include "kernel.h"

/* ---------------------------------------------------------------- timer -- */
volatile uint32_t ticks;
static unsigned hz_setting;

static void timer_irq(struct frame *f) {
    (void)f;
    ticks++;
    schedule(); /* preemptive round robin */
}

void timer_init(unsigned hz) {
    hz_setting = hz;
    uint32_t divisor = 1193182 / hz;
    outb(0x43, 0x36); /* channel 0, lo/hi byte, square wave */
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    irq_register(0, timer_irq);
}

unsigned timer_hz(void) { return hz_setting; }

/* ------------------------------------------------------------- keyboard -- */
#define KBD_BUF 64
static char kbd_buf[KBD_BUF];
static volatile unsigned kbd_head, kbd_tail;
static int shift_down;

static const char keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
};
static const char keymap_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
};

static void keyboard_irq(struct frame *f) {
    (void)f;
    uint8_t code = inb(0x60);
    if (code == 0x2A || code == 0x36) { shift_down = 1; return; }
    if (code == 0xAA || code == 0xB6) { shift_down = 0; return; }
    if (code & 0x80) return; /* key release */
    char c = shift_down ? keymap_shift[code] : keymap[code];
    if (!c) return;
    unsigned next = (kbd_head + 1) % KBD_BUF;
    if (next != kbd_tail) {
        kbd_buf[kbd_head] = c;
        kbd_head = next;
    }
}

void keyboard_init(void) { irq_register(1, keyboard_irq); }

int keyboard_getc(void) {
    if (kbd_head == kbd_tail) return -1;
    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF;
    return c;
}
