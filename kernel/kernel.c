/* Kernel entry point: bring up the machine, run self-checks, start the shell. */
#include "kernel.h"

extern uint8_t __kernel_end[];

static void selftest(void) {
    /* heap: allocate, free, coalesce */
    uint32_t before = heap_used();
    void *a = kmalloc(100);
    void *b = kmalloc(200);
    void *c = kmalloc(300);
    if (!a || !b || !c) panic("selftest: kmalloc failed");
    kfree(b);
    void *d = kmalloc(150); /* fits in the hole left by b */
    if (d != b) panic("selftest: first fit did not reuse the hole");
    kfree(a);
    kfree(c);
    kfree(d);
    if (heap_used() != before) panic("selftest: heap leak");
    /* frames: allocate two distinct frames and release them */
    void *f1 = frame_alloc();
    void *f2 = frame_alloc();
    if (!f1 || !f2 || f1 == f2 || ((uint32_t)f1 & 0xFFF)) panic("selftest: frame allocator");
    frame_free(f1);
    frame_free(f2);
    if (frames_free() != frames_total()) panic("selftest: frame leak");
    puts("selftest: memory ok\n");
}

void kmain(void) {
    serial_init();
    vga_init();
    puts("nanos 1.0 - a small operating system\n");
    kprintf("kernel: %u KiB, ends at %x\n", ((uint32_t)__kernel_end - 0x10000) / 1024, (uint32_t)__kernel_end);
    idt_init();
    mem_init();
    task_init();
    keyboard_init();
    timer_init(100);
    selftest();
    sti();
    kprintf("interrupts on, %u frames free, type help\n", frames_free());
    shell_run();
}
