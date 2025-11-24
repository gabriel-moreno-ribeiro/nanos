/* Interrupt descriptor table, exception reporting and the 8259 PICs. */
#include "kernel.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static irq_handler_t irq_handlers[16];
extern const uint32_t isr_table[48];
volatile uint32_t exception_count;

static const char *const exception_names[32] = {
    "divide error", "debug", "non-maskable interrupt", "breakpoint", "overflow", "bound range", "invalid opcode",
    "device not available", "double fault", "coprocessor overrun", "invalid TSS", "segment not present",
    "stack fault", "general protection fault", "page fault", "reserved", "x87 error", "alignment check",
    "machine check", "SIMD error", "virtualization", "control protection", "reserved", "reserved", "reserved",
    "reserved", "reserved", "reserved", "reserved", "reserved", "security", "reserved",
};

static void idt_set(int vector, uint32_t handler) {
    idt[vector].offset_low = handler & 0xFFFF;
    idt[vector].selector = 0x08;
    idt[vector].zero = 0;
    idt[vector].flags = 0x8E; /* present, ring 0, 32-bit interrupt gate */
    idt[vector].offset_high = handler >> 16;
}

static void pic_remap(void) {
    /* IRQ 0-7 -> vectors 32-39, IRQ 8-15 -> vectors 40-47 */
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFF); /* mask everything until a driver asks */
    outb(0xA1, 0xFF);
}

void idt_init(void) {
    for (int i = 0; i < 48; i++) idt_set(i, isr_table[i]);
    struct idt_ptr p = {sizeof(idt) - 1, (uint32_t)idt};
    __asm__ volatile("lidt %0" : : "m"(p));
    pic_remap();
}

void irq_register(unsigned irq, irq_handler_t handler) {
    irq_handlers[irq] = handler;
    if (irq < 8) {
        outb(0x21, inb(0x21) & ~(1 << irq));
    } else {
        outb(0xA1, inb(0xA1) & ~(1 << (irq - 8)));
        outb(0x21, inb(0x21) & ~(1 << 2)); /* cascade line */
    }
}

/* Called from the assembly stubs with a pointer to the saved registers. */
void isr_dispatch(struct frame *f) {
    if (f->vector < 32) {
        exception_count++;
        kprintf("\nexception %u (%s) at eip=%x error=%x\n", f->vector, exception_names[f->vector], f->eip, f->error);
        if (f->vector == 3 || f->vector == 4) return; /* traps: continue after the instruction */
        if (f->vector == 0) {
            /* divide error is a fault; skip the faulting instruction so the shell survives (2-byte idiv r/m32) */
            f->eip += 2;
            return;
        }
        panic("unrecoverable exception");
    }
    unsigned irq = f->vector - 32;
    if (irq >= 8) outb(0xA0, 0x20);
    outb(0x20, 0x20);
    if (irq_handlers[irq]) irq_handlers[irq](f);
}
